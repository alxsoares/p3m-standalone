/**    Copyright (C) 2011,2012,2013 Florian Weik <fweik@icp.uni-stuttgart.de>

       This program is free software: you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
       the Free Software Foundation, either version 3 of the License, or
       (at your option) any later version.

       This program is distributed in the hope that it will be useful,
       but WITHOUT ANY WARRANTY; without even the implied warranty of
       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
       GNU General Public License for more details.

       You should have received a copy of the GNU General Public License
       along with this program.  If not, see <http://www.gnu.org/licenses/>. **/

#include <stdio.h>
#include <string.h>
#include "types.h"
#include "interpol.h"
#include "charge-assign.h"

#ifdef c_ind
#undef c_ind
#define c_ind(A,B,C) (2*Mesh*Mesh*(A)+2*Mesh*(B)+2*(C))
#endif

int main(void) {
  double theMesh[2*32*32*32];
  double Charge1 = 1.0, Charge2 = 1.0;
  double Position1[3] = {0.0, 0.0, 0.0};
  double Position2[3] = {0.0, 0.0, 0.0};
  double caDings[2*7*7*7];
  int caind[2*3], i,j,k;
  double theForce, totalCharge=0.0;
  int MM = 31;
  Mesh =  32;
  cao = 7;
  ip = cao - 1;
  Len = 32.0;

  ca_ind[0] = caind;
  cf[0] = caDings;

  Interpolationspolynom_berechnen(ip);

  for(i=0.0;i<11;i++) {
    bzero(theMesh, 32*32*32*2*sizeof(double));
    Position1[0] = -0.5 + i*0.1;
    Position2[0] = 0.5 + i*0.1;
    if(Position1[0] < 0.0) 
      Position1[0] += Len;
    if(Position2[0] < 0.0)
      Position2[0] += Len;

    assign_charge(0, 1.0, Position1, theMesh, 0);
    assign_charge(1, -1.0, Position2, theMesh, 0);
    printf("%lf %lf %lf %e %lf\n", -0.5 + i*0.1, theMesh[c_ind(0,0,0)], theMesh[c_ind(31,0,0)], theMesh[c_ind(30,30,30)], LadInt[1][(int)(-0.5*i/(2.0*MaxInterpol))]);  
  }

  return 0;
}
