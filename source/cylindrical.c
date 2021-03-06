

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "atomic.h"

#include "python.h"


/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	cylin_ds_in_cell calculates the distance to the far
        boundary of the cell in which the photon bundle resides.  	
  
 Arguments:		
 	p	Photon pointer

	
 Returns:
 	Distance to the far boundary of the cell in which the photon
	currently resides.  Negative numbers (and zero) should be
	regarded as errors.
  
Description:	

Notes:


History:
 	04aug	ksl	This code was removed from translate_in_wind
       			in python_52 as part of effort to isolate
			dependencies on coordinate grids
 
**************************************************************/



double
cylind_ds_in_cell (p)
     PhotPtr p;


{

  int n, ix, iz, iroot;
  double a, b, c, root[2];
  double z1, z2, q;
  double smax;
  int ndom;

  ndom = wmain[p->grid].ndom;


  /* XXX -- The next lines may be unnecessary; they effectively recheck whether
   * the grid cell has changed, which should not be happening.  But we do have
   * the issue that we need to be sure to check what cell we are in at the right
   * time
   */


  if ((p->grid = n = where_in_grid (ndom, p->x)) < 0)
  {
    Error ("translate_in_wind: Photon not in grid when routine entered\n");
    return (n);                 /* Photon was not in wind */
  }

  wind_n_to_ij (ndom, n, &ix, &iz);     /*Convert the index n to two dimensions */

  smax = VERY_BIG;              //initialize smax to a large number

  /* Set up the quadratic equations in the radial rho direction */

  a = (p->lmn[0] * p->lmn[0] + p->lmn[1] * p->lmn[1]);
  b = 2. * (p->lmn[0] * p->x[0] + p->lmn[1] * p->x[1]);
  c = p->x[0] * p->x[0] + p->x[1] * p->x[1];

  iroot = quadratic (a, b, c - zdom[ndom].wind_x[ix] * zdom[ndom].wind_x[ix], root);    /* iroot will be the smallest positive root
                                                                                           if one exists or negative otherwise */

  if (iroot >= 0 && root[iroot] < smax)
    smax = root[iroot];

  iroot = quadratic (a, b, c - zdom[ndom].wind_x[ix + 1] * zdom[ndom].wind_x[ix + 1], root);

  if (iroot >= 0 && root[iroot] < smax)
    smax = root[iroot];

  /* At this point we have found how far the photon can travel in rho in its
     current direction.  Now we must worry about motion in the z direction  */

  z1 = zdom[ndom].wind_z[iz];
  z2 = zdom[ndom].wind_z[iz + 1];
  if (p->x[2] < 0)
  {                             /* We need to worry about which side of the plane the photon is on! */
    z1 *= (-1.);
    z2 *= (-1.);
  }

  if (p->lmn[2] != 0.0)
  {
    q = (z1 - p->x[2]) / p->lmn[2];
    if (q > 0 && q < smax)
      smax = q;
    q = (z2 - p->x[2]) / p->lmn[2];
    if (q > 0 && q < smax)
      smax = q;

  }

  return (smax);
}


/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	cylind_make_grid defines the cells in a cylindrical grid              

Arguments:		
	WindPtr w;	The structure which defines the wind in Python
 
Returns:
 
Description:


History:
	04aug	ksl	52a -- moved from define wind, and modeified so
			that calculates midpoints (xcen) as well as vertex
			points for cell.
	04dec	ksl	54a -- Minor mod to eliminate warnings when compiled
			with 03
	05jun	ksl	56d -- Fixed minor problem with linear grid for 
			w[n].xcen[2]

**************************************************************/


int
cylind_make_grid (ndom, w)
     WindPtr w;
     int ndom;
{
  double dr, dz, dlogr, dlogz, xfudge;
  int i, j, n;
  DomainPtr one_dom;


  one_dom = &zdom[ndom];

  if (zdom[ndom].zmax == 0)
  {
    /* Check if zmax has been set, and if not set it to rmax */
    zdom[ndom].zmax = zdom[ndom].rmax;
  }


  Log ("Making cylindrical grid %d\n", ndom);
  Log ("XXX rmax %e for domain %d\n", one_dom->rmax, ndom);

  /* In order to interpolate the velocity (and other) vectors out to geo.rmax, we need
     to define the wind at least one grid cell outside the region in which we want photons
     to propagate.  This is the reason we divide by NDIM-2 here, rather than NDIM-1 */


  /* First calculate parameters that are to be calculated at the edge of the grid cell.  This is
     mainly the positions and the velocity */
  for (i = 0; i < one_dom->ndim; i++)
  {
    for (j = 0; j < one_dom->mdim; j++)
    {
      wind_ij_to_n (ndom, i, j, &n);
      w[n].x[1] = w[n].xcen[1] = 0;     //The cells are all defined in the xz plane

      /*Define the grid points */
      if (one_dom->log_linear == 1)
      {                         // linear intervals

        dr = one_dom->rmax / (one_dom->ndim - 3);
        dz = one_dom->zmax / (one_dom->mdim - 3);
        w[n].x[0] = i * dr;     /* The first zone is at the inner radius of
                                   the wind */
        w[n].x[2] = j * dz;
        w[n].xcen[0] = w[n].x[0] + 0.5 * dr;
        w[n].xcen[2] = w[n].x[2] + 0.5 * dz;

      }
      else
      {                         //logarithmic intervals

        dlogr = (log10 (one_dom->rmax / one_dom->xlog_scale)) / (one_dom->ndim - 3);
        dlogz = (log10 (one_dom->zmax / one_dom->zlog_scale)) / (one_dom->mdim - 3);

        if (dlogr <= 0)
        {
          Error ("cylindrical: dlogr %g is less than 0.  This is certainly wrong! Aborting\n", dlogr);
          exit (0);
        }

        if (dlogz <= 0)
        {
          Error ("cylindrical: dlogz %g is less than 0.  This is certainly wrong! Aborting\n", dlogz);
          exit (0);
        }

        if (i == 0)
        {
          w[n].x[0] = 0.0;
          w[n].xcen[0] = 0.5 * one_dom->xlog_scale;
        }
        else
        {
          w[n].x[0] = one_dom->xlog_scale * pow (10., dlogr * (i - 1));
          w[n].xcen[0] = 0.5 * one_dom->xlog_scale * (pow (10., dlogr * (i - 1)) + pow (10., dlogr * (i)));
        }

        if (j == 0)
        {
          w[n].x[2] = 0.0;
          w[n].xcen[2] = 0.5 * one_dom->zlog_scale;
        }
        else
        {
          w[n].x[2] = one_dom->zlog_scale * pow (10, dlogz * (j - 1));
          w[n].xcen[2] = 0.5 * one_dom->zlog_scale * (pow (10., dlogz * (j - 1)) + pow (10., dlogz * (j)));
        }
      }

      xfudge = fmin ((w[n].xcen[0] - w[n].x[0]), (w[n].xcen[2] - w[n].x[2]));
      w[n].dfudge = XFUDGE * xfudge;

    }
  }

  return (0);
}


/* This simple little routine just populates two one dimensional arrays that are used for interpolation.
 * It could be part of the routine above, except that the arrays are  not tranferred to py_wind in wind_save
 * It's left that way for now, but when one cleans up the program, it might be more sensible to do it the other
 * way
 *
History
04aug	ksl	Routine was removed from windsave,  wind_complete is now just a driver.
15sep	ksl	Update for domains
 */

int
cylind_wind_complete (ndom, w)
     int ndom;
     WindPtr w;
{
  int i, j;
  int nstart, mdim, ndim;
  DomainPtr one_dom;

  one_dom = &zdom[ndom];
  nstart = one_dom->nstart;
  ndim = one_dom->ndim;
  mdim = one_dom->mdim;

  /* Finally define some one-d vectors that make it easier to locate a photon in the wind given that we
     have adoped a "rectangular" grid of points.  Note that rectangular does not mean equally spaced. */

  for (i = 0; i < ndim; i++)
    one_dom->wind_x[i] = w[nstart + i * mdim].x[0];

  for (j = 0; j < one_dom->mdim; j++)
    one_dom->wind_z[j] = w[nstart + j].x[2];

  for (i = 0; i < one_dom->ndim - 1; i++)
    one_dom->wind_midx[i] = 0.5 * (w[nstart + i * mdim].x[0] + w[nstart + (i + 1) * mdim].x[0]);

  for (j = 0; j < one_dom->mdim - 1; j++)
    one_dom->wind_midz[j] = 0.5 * (w[nstart + j].x[2] + w[nstart + j + 1].x[2]);

  /* Add something plausible for the edges */
  one_dom->wind_midx[one_dom->ndim - 1] = 2. * one_dom->wind_x[nstart + ndim - 1] - one_dom->wind_midx[nstart + ndim - 2];
  one_dom->wind_midz[one_dom->mdim - 1] = 2. * one_dom->wind_z[nstart + mdim - 1] - one_dom->wind_midz[nstart + mdim - 2];

  return (0);
}

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
 	cylind_volume(w) calculates the wind volume of a cylindrical cell
	allowing for the fact that some cells 

 Arguments:		
	WindPtr w;    the entire wind
 Returns:

 Description:
	This is a brute-froce integration of the volume 

	ksl--04aug--some of the machinations regarding what to to at the 
	edge of the wind seem bizarre, like a substiture for figuring out
	what one actually should be doing.  However, volume > 0 is used
	for making certain choices in the existing code, and so one does
	need to be careful.  
	
		
 Notes:
	Where_in grid does not tell you whether the photon is in the wind or not. 
 History:
 	04aug	ksl	52a -- Moved from wind2d
	04aug	ksl	52a -- Made the weighting of the integration accurate. In 
			the process, I introduced variables to make the code
			more readable.
	05apr	ksl	55d -- Modified to include the determination of whether
			a cell was completely in the wind or not.  This
			functionality had been in define_wind.
	06nov	ksl	58b: Minor modification to use W_ALL_INWIND, etc.
			instead of hardcoded values
	09mar	ksl	68c: Modified so that integration is only carried
			out when the cell is partially in the wind as
			evidenced by the fact that only some of the
			corners of the cell are in the wind.  The intent
			is to avoid wasting time
	11aug	ksl	Add ability to determine the volume for different
			components.  See python.h for explanation of
			relatinship between PART and ALL
	15sep	ksl	Modified to account for mulitple domains
 
**************************************************************/

#define RESOLUTION   1000


int
cylind_volumes (ndom, w)
     int ndom;                  // domain number
     WindPtr w;
{
  int i, j, n;
  int jj, kk;
  double fraction;
  double num, denom;
  double r, z;
  double rmax, rmin;
  double zmin, zmax;
  double dr, dz, x[3];
  int n_inwind, ndomain;
  DomainPtr one_dom;

  one_dom = &zdom[ndom];


  for (i = 0; i < one_dom->ndim; i++)
  {
    for (j = 0; j < one_dom->mdim; j++)
    {

      wind_ij_to_n (ndom, i, j, &n);

      // XXX Why is it necessary to do the check indicated by the if statement.   
      /* 70b - only try to assign the cell if it has not already been assigned */
      if (w[n].inwind == W_NOT_INWIND)
      {

        rmin = one_dom->wind_x[i];
        rmax = one_dom->wind_x[i + 1];
        zmin = one_dom->wind_z[j];
        zmax = one_dom->wind_z[j + 1];

        //leading factor of 2 added to allow for volume above and below plane (SSMay04)
        w[n].vol = 2 * PI * (rmax * rmax - rmin * rmin) * (zmax - zmin);

        n_inwind = cylind_is_cell_in_wind (n);

        if (n_inwind == W_NOT_INWIND)
        {
          fraction = 0.0;
          jj = 0;
          kk = RESOLUTION * RESOLUTION;
        }
        else if (n_inwind == W_ALL_INWIND)
        {
          fraction = 1.0;
          jj = kk = RESOLUTION * RESOLUTION;
        }
        else
        {                       /* Determine whether the grid cell is in the wind */
          num = denom = 0;
          jj = kk = 0;
          dr = (rmax - rmin) / RESOLUTION;
          dz = (zmax - zmin) / RESOLUTION;
          for (r = rmin + dr / 2; r < rmax; r += dr)
          {
            for (z = zmin + dz / 2; z < zmax; z += dz)
            {
              denom += r * r;
              kk++;
              x[0] = r;
              x[1] = 0;
              x[2] = z;

              if (where_in_wind (x, &ndomain) == W_ALL_INWIND && ndom == ndomain)
              {
                num += r * r;   /* 0 implies in wind */
                jj++;
              }
            }
          }
          fraction = num / denom;


        }

        /* OK now make the final assignement of nwind and fix the volumes */
        if (jj == 0)
        {
          w[n].inwind = W_NOT_INWIND;   // The cell is not in the wind
          w[n].vol = 0.0;
        }
        else if (jj == kk)
        {
          w[n].inwind = W_ALL_INWIND;   // All of cell is inwind
        }
        else
        {
          w[n].inwind = W_PART_INWIND;  // Some of cell is inwind
          w[n].vol *= fraction;
        }
      }
    }
  }

  return (0);
}

/***********************************************************
                     Space Telescope Science Institute

 Synopsis:
 	cylind_where_in_grid locates the grid position of the vector,
	when one is using cylindrical coordinates.   

 Arguments:		
	double x[];
 Returns:
 	where_in_grid normally  returns the element number in wmain associated with
 		a position.  If the position is in the grid this will be a positive
 		integer 
 	x is inside the grid        -1
	x is outside the grid       -2
 Description:	
	
		
 Notes:
	Where_in grid does not tell you whether the x is in the wind or not. 

	What one means by inside or outside the grid may well be different
	for different coordinate systems.



 History:
	04aug	ksl	52a -- Moved from where_in_wind as incorporated
			multiple coordinate systems
  	13sep	nsh	76b -- changed calls to fraction to take account
			of multiple modes.
 
**************************************************************/



int
cylind_where_in_grid (ndom, x)
     int ndom;
     double x[];
{
  int i, j, n;
  double z;
  double rho;
  double f;
  DomainPtr one_dom;

  one_dom = &zdom[ndom];

  z = fabs (x[2]);              /* This is necessary to get correct answer above
                                   and below plane */
  if (z == 0)
    z = 1.e4;                   //Force z to be positive  02feb ksl
  rho = sqrt (x[0] * x[0] + x[1] * x[1]);       /* This is distance from z
                                                   axis */
  /* Check to see if x is outside the region of the calculation */
  if (rho > one_dom->wind_x[one_dom->ndim - 1] || z > one_dom->wind_z[one_dom->mdim - 1])
  {
    return (-2);                /* x is outside grid */
  }

  if (rho < one_dom->wind_x[0])
    return (-1);

  fraction (rho, one_dom->wind_x, one_dom->ndim, &i, &f, 0);
  fraction (z, one_dom->wind_z, one_dom->mdim, &j, &f, 0);

  /* At this point i,j are just outside the x position */
  wind_ij_to_n (ndom, i, j, &n);

  /* n is the array element in wmain */
  return (n);
}



/***********************************************************
                     Space Telescope Science Institute

 Synopsis:
 	cylind_get_random_location

 Arguments:		
 	int n -- Cell in which random poition is to be generated
	int ndom - The component, e. g. the wind in which the
		location is to be generated.
 Returns:
 	double x -- the position
 Description:	
	
		
 Notes:



 History:
	04aug	ksl	52a -- Moved from where_in_wind as incorporated
			multiple coordinate systems
	15sep	ksl	Modified so that the random location has to be
			in the wind in a particular domain
 
**************************************************************/

int
cylind_get_random_location (n, x)
     int n;                     // Cell in which to create postion
     double x[];                // Returned position
{
  int i, j;
  int inwind;
  double r, rmin, rmax, zmin, zmax;
  double zz;
  double phi;
  int ndom, ndomain;
  DomainPtr one_dom;

  ndom = wmain[n].ndom;
  ndomain = -1;
  one_dom = &zdom[ndom];

  wind_n_to_ij (ndom, n, &i, &j);

  rmin = one_dom->wind_x[i];
  rmax = one_dom->wind_x[i + 1];
  zmin = one_dom->wind_z[j];
  zmax = one_dom->wind_z[j + 1];

  /* Generate a position which is both in the cell and in the wind */
  inwind = W_NOT_INWIND;
  while (inwind != W_ALL_INWIND || ndomain != ndom)
  {
    r = sqrt (rmin * rmin + (rand () / (MAXRAND - 0.5)) * (rmax * rmax - rmin * rmin));

// Generate the azimuthal location
    phi = 2. * PI * (rand () / MAXRAND);
    x[0] = r * cos (phi);
    x[1] = r * sin (phi);



    x[2] = zmin + (zmax - zmin) * (rand () / (MAXRAND - 0.5));
    inwind = where_in_wind (x, &ndomain);       /* Some photons will not be in the wind
                                                   because the boundaries of the wind split the grid cell */
  }

  zz = rand () / MAXRAND - 0.5; //positions above are all at +z distances

  if (zz < 0)
    x[2] *= -1;                 /* The photon is in the bottom half of the wind */

  return (inwind);
}


/***********************************************************
                     Space Telescope Science Institute

 Synopsis:
 	cylind_extend_density  extends the density to
	regions just outside the wind regiions so that
	extrapolations of density can be made there

 Arguments:		
 Returns:
 Description:	
	
		
 Notes:



 History:
	05apr	ksl	56 -- Moved functionality from wind updates   
	06may	ksl	57+ -- Changed to a mapping for plasma.  The
				idea is that we always can use a mapping
				to define what is in an adjascent cell.
				This will need to be verified.
	15aug	ksl	Modified for multipel domains
 
**************************************************************/


int
cylind_extend_density (ndom, w)
     WindPtr w;
{

  int i, j, n, m;
  int ndim, mdim;

  ndim = zdom[ndom].ndim;
  mdim = zdom[ndom].mdim;

  /* Now we need to updated the densities immediately outside the wind so that the density interpolation in resonate will work.
     In this case all we have done is to copy the densities from the cell which is just in the wind (as one goes outward) to the
     cell that is just inside (or outside) the wind. 

     SS asked whether we should also be extending the wind for other parameters, especially ne.  At present we do not interpolate
     on ne so this is not necessary.  If we did do that it would be required.

     In cylindrical coordinates, the fast dimension is z; grid positions increase up in z, and then out in r.
     In spperical polar coordinates, the fast dimension is theta; the grid increases in theta (measured)
     from the z axis), and then in r.
     In spherical coordinates, the grid increases as one might expect in r..
     *
   */

  for (i = 0; i < ndim - 1; i++)
  {
    for (j = 0; j < mdim - 1; j++)
    {
      wind_ij_to_n (ndom, i, j, &n);
      if (w[n].vol == 0)

      {                         //Then this grid point is not in the wind 

        wind_ij_to_n (ndom, i + 1, j, &m);
        if (w[m].vol > 0)
        {                       //Then the windcell in the +x direction is in the wind and
          // we can copy the densities to the grid cell n
          w[n].nplasma = w[m].nplasma;

        }
        else if (i > 0)
        {
          wind_ij_to_n (ndom, i - 1, j, &m);
          if (w[m].vol > 0)
          {                     //Then the grid cell in the -x direction is in the wind and
            // we can copy the densities to the grid cell n
            w[n].nplasma = w[m].nplasma;

          }
        }
      }
    }
  }

  return (0);

}

/*

cylind_is_cell_in_wind (n)

This routine performs is a robust check of whether a cell is in the wind or not.  
It was created to speed up the evaluation of the volumes for the wind.  It
checks each of the four boundaries of the wind to see whether any portions
of these are in the wind

Note that it simply calls where_in_wind multiple times.

History:
  11Aug	ksl	70b - Modified to incoporate torus
  		See python.h for more complete explanation
		of how PART and ALL are related
  15sep ksl	Modified to ask the more refined question of
  		whether this cell is in the wind of the
		specific domain assigned to the cell.
	

*/

int
cylind_is_cell_in_wind (n)
     int n;                     // cell number
{
  int i, j;
  double r, z, dr, dz;
  double rmin, rmax, zmin, zmax;
  double x[3];
  int ndom, ndomain;
  DomainPtr one_dom;

  ndom = wmain[n].ndom;
  one_dom = &zdom[ndom];
  wind_n_to_ij (ndom, n, &i, &j);

  /* First check if the cell is in the boundary */
  if (i >= (one_dom->ndim - 2) && j >= (one_dom->mdim - 2))
  {
    return (W_NOT_INWIND);
  }

  /* Assume that if all four corners are in the wind that the
     entire cell is in the wind.  check_corners also now checks 
     that the corners are in the wind of a specific domain */

  if (check_corners_inwind (n) == 4)
  {
    return (W_ALL_INWIND);
  }

  /* So at this point, we have dealt with the easy cases 
     of being in the region of the wind which is used for the 
     boundary and when all the edges of the cell are in the wind.
   */


  rmin = one_dom->wind_x[i];
  rmax = one_dom->wind_x[i + 1];
  zmin = one_dom->wind_z[j];
  zmax = one_dom->wind_z[j + 1];

  dr = (rmax - rmin) / RESOLUTION;
  dz = (zmax - zmin) / RESOLUTION;

  // Check inner and outer boundary in the z direction

  x[1] = 0;

  for (z = zmin + dz / 2; z < zmax; z += dz)
  {
    x[2] = z;

    x[0] = rmin;

    if (where_in_wind (x, &ndomain) == W_ALL_INWIND && ndom == ndomain)
    {
      return (W_PART_INWIND);
    }

    x[0] = rmax;

    if (where_in_wind (x, &ndomain) == W_ALL_INWIND && ndom == ndomain)
    {
      return (W_PART_INWIND);
    }
  }


  // Check inner and outer boundary in the z direction

  for (r = rmin + dr / 2; r < rmax; r += dr)
  {

    x[0] = r;

    x[2] = zmin;

    if (where_in_wind (x, &ndomain) == W_ALL_INWIND && ndom == ndomain)
    {
      return (W_PART_INWIND);
    }

    x[2] = zmax;

    if (where_in_wind (x, &ndomain) == W_ALL_INWIND && ndom == ndomain)
    {
      return (W_PART_INWIND);
    }
  }

  /* If one has reached this point, then this wind cell is not in the wind */
  return (W_NOT_INWIND);
}
