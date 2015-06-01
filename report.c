
#include "paul.h"

void planetaryForce( struct planet * , double , double , double , double * , double * , double * , int );

double get_dV( double * , double * );

void report( struct domain * theDomain ){

   double t = theDomain->t;
   struct cell ** theCells = theDomain->theCells;
   int Nr = theDomain->Nr;
   int Nz = theDomain->Nz;
   int * Np = theDomain->Np;
   int Ng = theDomain->Ng;
   int rank = theDomain->rank;
   int * dim_rank = theDomain->dim_rank;
   int * dim_size = theDomain->dim_size;
   MPI_Comm grid_comm = theDomain->theComm;

   struct planet * thePlanets = theDomain->thePlanets;
   int Npl = theDomain->Npl;

   double r_p = 0.0;
   if( Npl > 1 ) r_p = thePlanets[1].r;

   double * r_jph = theDomain->r_jph;
   double * z_kph = theDomain->z_kph;

   int jmin = Ng;
   int jmax = Nr-Ng;
   if( dim_rank[0]==0             ) jmin = 0;
   if( dim_rank[0]==dim_size[0]-1 ) jmax = Nr;
   int kmin = Ng;
   int kmax = Nz-Ng;
   if( dim_rank[1]==0             ) kmin = 0;
   if( dim_rank[1]==dim_size[1]-1 ) kmax = Nz;

   int j,k,i;
   double L1_isen = 0.0;
   double L1_rho  = 0.0;
   double L1_P    = 0.0;
   double Power  = 0.0;
   double Torque = 0.0;
   double Torque2 = 0.0;
   double PsiR = 0.0;
   double PsiI = 0.0;
   double Vol = 0.0;
   double rho_min = HUGE_VAL;
   double rhoavg_min = HUGE_VAL;

   //double T_cut[10];
   //double P_cut[10];
   //for( j=0 ; j<10 ; ++j ){ T_cut[j]=0.;  P_cut[j]=0.; }

   for( j=jmin ; j<jmax ; ++j ){
      double r = .5*(r_jph[j]+r_jph[j-1]);
      double rho0 = 1.0;//pow( r , -1.5 );
      double rho_avg = 0.0;
      double Vol_avg = 0.0;
      for( k=kmin ; k<kmax ; ++k ){
         int jk = j+Nr*k;
         for( i=0 ; i<Np[jk] ; ++i ){
            struct cell * c = &(theCells[jk][i]);
            double phi = c->piph - .5*c->dphi;
            double Pp  = c->prim[PPP];
            double rho = c->prim[RHO];

            double phip = c->piph;
            double phim = phip-c->dphi;
            double xp[3] = {r_jph[j]  ,phip,z_kph[k]  };
            double xm[3] = {r_jph[j-1],phim,z_kph[k-1]};
            double dV = get_dV( xp , xm );

            PsiR += rho*dV*cos(phi);
            PsiI += rho*dV*sin(phi);
 
            L1_isen += fabs(Pp/pow(rho,5./3.)-1.)*dV;
            L1_rho  += fabs(rho/rho0-1.)*dV;
            L1_P    += fabs(Pp/pow(rho,5./3.)/0.01-1.)*dV;
            Vol += dV;

            if( rho_min > rho/rho0 ) rho_min = rho/rho0;
            rho_avg += rho*dV;
            Vol_avg += dV;

            if( Npl > 1 ){
               double fr,fp,fz,fp2;
               double rp = thePlanets[1].r;
               double om = thePlanets[1].omega;
               double vr = thePlanets[1].vr;
               //double mp = thePlanets[1].M;
               planetaryForce( thePlanets   , r , phi , 0.0 , &fr , &fp2 , &fz , 1 );
               planetaryForce( thePlanets+1 , r , phi , 0.0 , &fr , &fp  , &fz , 1 );

               //double pp = thePlanets[1].phi;
               //double cosp = cos(phi);
               //double sinp = sin(phi);
               //double dx = r*cosp-rp*cos(pp);
               //double dy = r*sinp-rp*sin(pp);
               //double script_r = sqrt(dx*dx+dy*dy);
               //double rH = pow( thePlanets[1].M/3. , 1./3. );

               Torque -= (rho-1.0)*rp*fp*dV;
               Power  -= (rho-1.0)*( rp*om*fp + vr*fr )*dV;
               Torque2 -= (rho-1.0)*rp*fp2*dV;
/*
               int n_cut;
               for( n_cut=0 ; n_cut<10 ; ++n_cut ){
                  double r_cut = 0.3*((double)(n_cut+1.)/10.);
                  double scriptr2 = rp*rp + r*r - 2.*r*rp*cos(phi-pp);
                  if( scriptr2 > r_cut*r_cut ){
                     T_cut[n_cut] -= (rho-1.0)*rp*fp*dV;
                     P_cut[n_cut] -= (rho-1.0)*( rp*om*fp + vr*fr )*dV;
                  }
               }
*/
            }
         }
      }
      rho_avg /= Vol_avg;
      if( rhoavg_min > rho_avg/rho0 ) rhoavg_min = rho_avg/rho0;
   }

   MPI_Allreduce( MPI_IN_PLACE , &L1_isen , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &L1_rho  , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &L1_P    , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &Vol     , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &Torque  , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &Torque2 , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &Power   , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &PsiR    , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &PsiI    , 1 , MPI_DOUBLE , MPI_SUM , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &rho_min    , 1 , MPI_DOUBLE , MPI_MIN , grid_comm );
   MPI_Allreduce( MPI_IN_PLACE , &rhoavg_min , 1 , MPI_DOUBLE , MPI_MIN , grid_comm );

//   MPI_Allreduce( MPI_IN_PLACE , T_cut  , 10 , MPI_DOUBLE , MPI_SUM , grid_comm );
//   MPI_Allreduce( MPI_IN_PLACE , P_cut  , 10 , MPI_DOUBLE , MPI_SUM , grid_comm );

   L1_isen /= Vol;
   L1_rho  /= Vol;
   L1_P    /= Vol;

   if( rank==0 ){
      FILE * rFile = fopen("report.dat","a");
      fprintf(rFile,"%e %e %e %e %e %e %e %e %e\n",t,Torque,Power,r_p,rho_min,rhoavg_min,PsiR,PsiI,Torque2);
      //fprintf(rFile,"%e %e %e ",t,Torque,Power);
      //for( j=0 ; j<10 ; ++j ) fprintf(rFile,"%e %e ",T_cut[j],P_cut[j]);
      //fprintf(rFile,"\n");
      fclose(rFile);
   }

}