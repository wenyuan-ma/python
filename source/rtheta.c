

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "atomic.h"
#include "python.h"

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	rtheta_ds_in_cell calculates the distance to the far
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
	15aug	ksl	Updates to where_in_grid section to allow
			for multiple domains
 
**************************************************************/



double
rtheta_ds_in_cell (p)
     PhotPtr p;


{

  int n, ix, iz;
  double s, smax;
  int ndom;

  ndom = wmain[p->grid].ndom;


  /* XXX Note clear that next lines are necessary as they effectively recheck
   * what grid cell a photon is in */

  if ((p->grid = n = where_in_grid (ndom, p->x)) < 0)
  {
    Error ("translate_in_wind: Photon not in grid when routine entered\n");
    return (n);                 /* Photon was not in wind */
  }

  wind_n_to_ij (ndom, n, &ix, &iz);     /*Convert the index n to two dimensions */


  /* Set up the quadratic equations in the radial  direction */

  smax = ds_to_sphere (zdom[ndom].wind_x[ix], p);
  s = ds_to_sphere (zdom[ndom].wind_x[ix + 1], p);
  if (s < smax)
  {
    smax = s;
  }

  /* At this point we have found how far the photon can travel in r in its
     current direction.  Now we must worry about motion in the theta direction  */

  s = ds_to_cone (&zdom[ndom].cones_rtheta[iz], p);
  if (s < smax)
  {
    smax = s;
  }

  s = ds_to_cone (&zdom[ndom].cones_rtheta[iz + 1], p);
  if (s < smax)
  {
    smax = s;
  }

  if (smax <= 0)
  {
    Error ("rtheta: ds_in_cell %f\n", smax);
  }
  return (smax);
}



/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	rtheta_make_grid defines the cells in a rtheta grid              

Arguments:		
	WindPtr w;	The structure which defines the wind in Python
 
Returns:
 
Description:

	In the implementation of cylindrical coordinates, we have defined
	the wind so that as (n = i * MDIM + j) is incremented, the positions 
	move in the z direction and then in the x direction, so that that 
	[i][j] correspond to the x, z position of the cell.

	For spherical polar, components we have defined the grid so that
	i corresponds to a radial distance, and j corresponds to an angle,
	e.g r theta.  increasing i corresponds to increasing distance,
	and increasing theta corresponds to increasing angle measured from
	the z axis. (This it should be fairly easy to implement true 
	spherical coordinates in the future.

	For now, we will assume that logarithmic intervals only affect the
	radial direction and do not affect theta.  It is not obvious that
	is what you want, and it is also not obvious that when you have 
	a vertically extended disk that theta should go over the full 90
	degrees.


History:
	04aug	ksl	52a -- Coded and debugged.
	04dec	ksl	54a -- Made minor change to eliminate warning
			when compiled with 03
	13jun	ksl	Modify the rtheta grid so that no cells extend
			into the -z plane.  There were problems associted
			with the fact that somoe of the dummy cells
			extended into the -z plane. This change amoutns to changing 
			the way the boundary contintion are set up.

**************************************************************/


int
rtheta_make_grid (w, ndom)
     WindPtr w;
     int ndom;
{
  double dr, theta, thetacen, dtheta, dlogr;
  int i, j, n;
  int mdim, ndim;

  /* This lines are a catchall just ensure that zmax is set to something
   * sensible as is done for the cylindrical grid.  zmax should have 
   * been defined previously.  ksl 170506
   */

   if (zdom[ndom].zmax == 0)
	{
	/* Check if zmax has been set, and if not set it to rmax */
	zdom[ndom].zmax = zdom[ndom].rmax;
	}

  ndim = zdom[ndom].ndim;
  mdim = zdom[ndom].mdim;

  /* In order to interpolate the velocity (and other) vectors out to geo.rmax, we need
     to define the wind at least one grid cell outside the region in which we want photons
     to propagate.  This is the reason we divide by NDIM-2 here, rather than NDIM-1 */

/* Next two lines for linear intervals */
  dtheta = 90. / (mdim - 3);


  /* First calculate parameters that are to be calculated at the edge of the grid cell.  This is
     mainly the positions and the velocity */
  for (i = 0; i < ndim; i++)
  {
    for (j = 0; j < mdim; j++)
    {
      wind_ij_to_n (ndom, i, j, &n);


      /*Define the grid points */
      if (zdom[ndom].log_linear == 1)
      {                         // linear intervals

        dr = (zdom[ndom].rmax - geo.rstar) / (ndim - 3);
        w[n].r = geo.rstar + i * dr;
        w[n].rcen = w[n].r + 0.5 * dr;
      }
      else
      {                         //logarithmic intervals

        dlogr = (log10 (zdom[ndom].rmax / geo.rstar)) / (mdim - 3);
        w[n].r = geo.rstar * pow (10., dlogr * (i - 1));
        w[n].rcen = 0.5 * geo.rstar * (pow (10., dlogr * (i)) + pow (10., dlogr * (i - 1)));
      }

      /* Only the radial distance can be logarithmic */

      theta = w[n].theta = dtheta * j;
      thetacen = w[n].thetacen = w[n].theta + 0.5 * dtheta;
      if (theta > 90.)
      {
        theta = 90.;
      }
      if (thetacen > 90.)
      {
        thetacen = 90.;
      }


      /* Now calculate the positions of these points in the xz plane */
      theta /= RADIAN;
      thetacen /= RADIAN;
      w[n].x[1] = w[n].xcen[1] = 0.0;

      w[n].x[0] = w[n].r * sin (theta);
      w[n].x[2] = w[n].r * cos (theta);

      w[n].xcen[0] = w[n].rcen * sin (thetacen);
      w[n].xcen[2] = w[n].rcen * cos (thetacen);

    }
  }
  rtheta_make_cones (ndom, w);
  return (0);
}




/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	rtheta_make_cones defines the wind cones that are needed to calculate ds in a cell              

Arguments:		
	WindPtr w;	The structure which defines the wind in Python
 
Returns:
 
Description:

	This subroutine defines the edges of the wind


History:
	13Aug	nsh	76b- Broken this small routine out of rtheta_make_grid
			in order to allow proga.c to use the same commands,
			This was also to fix issue41 whereby a model with
			an rtheta grid could not be restarted. This was because
			these commands are only called when the initial
			model is set up - and the cones structure is not saved.
	15aug	ksl	Moved cones to domains
			

**************************************************************/


  /* Now set up the wind cones that are needed for calclating ds in a cell */

int
rtheta_make_cones (ndom, w)
     int ndom;
     WindPtr w;
{
  int n;
  int mdim;

  mdim = zdom[ndom].mdim;


  zdom[ndom].cones_rtheta = (ConePtr) calloc (sizeof (cone_dummy), mdim);
  if (zdom[ndom].cones_rtheta == NULL)
  {
    Error ("rtheta_make_gid: There is a problem in allocating memory for the cones structure\n");
    exit (0);

  }


  for (n = 0; n < mdim; n++)
  {
    zdom[ndom].cones_rtheta[n].z = 0.0;
    zdom[ndom].cones_rtheta[n].dzdr = 1. / tan (w[n].theta / RADIAN);   // New definition
  }


  return (0);
}








/* 
 
rtheta_wind_complete (w)

Description

This simple little routine just populates two one dimensional arrays that are used for interpolation.
It could be part of the routine above, except that the arrays are  not tranferred to py_wind in wind_save
It's left that way for now, but when one cleans up the program, it might be more sensible to do it the other
way

Note that because of history these interpolation variable look like those for cylindrical coordinates.
It would be better to rename the variables at some point for clarity!! ksl

History
	04aug	ksl	Routine was removed from windsave,  wind_complete is now just a driver.
	15aug	ksl	Modified for domains
 */

int
rtheta_wind_complete (ndom, w)
     int ndom;
     WindPtr w;
{
  int i, j;
  int ndim, mdim, nstart;

  ndim = zdom[ndom].ndim;
  mdim = zdom[ndom].mdim;
  nstart = zdom[ndom].nstart;



  /* Finally define some one-d vectors that make it easier to locate a photon in the wind given that we
     have adoped a "rectangular" grid of points.  Note that rectangular does not mean equally spaced. */

  for (i = 0; i < ndim; i++)
  {
    zdom[ndom].wind_x[i] = w[nstart + i * mdim].r;
  }
  for (j = 0; j < mdim; j++)
    zdom[ndom].wind_z[j] = w[nstart + j].theta;

  for (i = 0; i < ndim - 1; i++)
    zdom[ndom].wind_midx[i] = w[nstart + i * mdim].rcen;

  for (j = 0; j < mdim - 1; j++)
    zdom[ndom].wind_midz[j] = w[nstart + j].thetacen;

  /* Add something plausible for the edges */
  /* ?? It is bizarre that one needs to do anything like this ???. wind should be defined to include NDIM -1 */
  zdom[ndom].wind_midx[ndim - 1] = 2. * zdom[ndom].wind_x[ndim - 1] - zdom[ndom].wind_midx[ndim - 2];
  zdom[ndom].wind_midz[mdim - 1] = 2. * zdom[ndom].wind_z[mdim - 1] - zdom[ndom].wind_midz[mdim - 2];

  return (0);
}


/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
 	rtheta_volume(w) calculates the wind volume of a cylindrical cell
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
	need to be careful.  The existing code does not accurately calcuate
	the volume in the sense that it does not weight accroding to rho!
		
 Notes:
	Where_in grid does not tell you whether the photon is in the wind or not. 
 History:
 	04aug	ksl	52a -- Moved from wind2d
	05apr   ksl     55d -- Modified to include the determination of whether
			a cell was completely in the wind or not.  This
			functionality had been in define_wind.
	06nov	ksl	58b -- Minor modification to use defined variables
			W_ALL_INWIND, etc. instead of hardcoded vlues
	03apr	ksl	68c -- Added robust check of whether a cell was
			in the wind or not to speed up the volume
			calculation (and allow one to use a high resolution
			for the numerical integration when a cell is
			partially in the wind
	11aug	ksl	70b -- Added possibility of finding the volumes
			in multiple components. See python.h for discussion
			of the relationship betwedn PART and LLL

 
**************************************************************/
#define RESOLUTION   1000



int
rtheta_volumes (ndom, w)
     int ndom;
     WindPtr w;
{
  int i, j, n;
  int jj, kk;
  double fraction;
  double num, denom;
  double r, theta;
  double dr, dtheta, x[3];
  double rmin, rmax, thetamin, thetamax;
  int n_inwind;
  int ndim, mdim, ndomain;

  ndim = zdom[ndom].ndim;
  mdim = zdom[ndom].mdim;

  for (i = 0; i < ndim; i++)
  {
    for (j = 0; j < mdim; j++)
    {
      wind_ij_to_n (ndom, i, j, &n);
      if (w[n].inwind == W_NOT_INWIND)
      {

        rmin = zdom[ndom].wind_x[i];
        rmax = zdom[ndom].wind_x[i + 1];
        thetamin = zdom[ndom].wind_z[j] / RADIAN;
        thetamax = zdom[ndom].wind_z[j + 1] / RADIAN;

        //leading factor of 2 added to allow for volume above and below plane (SSMay04)
        w[n].vol = 2. * 2. / 3. * PI * (rmax * rmax * rmax - rmin * rmin * rmin) * (cos (thetamin) - cos (thetamax));

        n_inwind = rtheta_is_cell_in_wind (n);
        if (n_inwind == W_NOT_INWIND)
        {
          fraction = 0.0;       /* Force outside edge volues to zero */
          jj = 0;
          kk = RESOLUTION * RESOLUTION;
        }
        else if (n_inwind == W_ALL_INWIND)
        {
          fraction = 1.0;       /* Force outside edge volues to zero */
          jj = kk = RESOLUTION * RESOLUTION;
        }


        else
        {                       /* The grid cell is PARTIALLY in the wind */
          num = denom = 0;
          jj = kk = 0;
          dr = (rmax - rmin) / RESOLUTION;
          dtheta = (thetamax - thetamin) / RESOLUTION;
          for (r = rmin + dr / 2; r < rmax; r += dr)
          {
            for (theta = thetamin + dtheta / 2; theta < thetamax; theta += dtheta)
            {
              denom += r * r * sin (theta);;
              kk++;
              x[0] = r * sin (theta);
              x[1] = 0;
              x[2] = r * cos (theta);;
              if (where_in_wind (x, &ndomain) == W_ALL_INWIND)
              {
                num += r * r * sin (theta);     /* 0 implies in wind */
                jj++;
              }
            }
          }
          fraction = num / denom;
        }

        /* OK now assign inwind value and final volumes */

        if (jj == 0)
        {
          w[n].inwind = W_NOT_INWIND;   // The cell is not in the wind
          w[n].vol = 0.0;
        }
        else if (jj == kk)
          w[n].inwind = W_ALL_INWIND;   // The cell is completely in the wind
        else
        {
          w[n].inwind = W_PART_INWIND;  //The cell is partially in the wind
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
 	rtheta_where_in_grid locates the grid position of the vector,
	when one is using rtheta coordinates. 

 Arguments:		
	double x[];
 Returns:
 	where_in_grid normally  returns the cell number associated with
 		a position.  If the position is in the grid this will be a positive
 		integer < NDIM*MDIM.
 	x is inside the grid        -1
	x is outside the grid       -2
 Description:	
	
		
 Notes:
	Where_in grid does not tell you whether the x is in the wind or not. 

	What one means by inside or outside the grid may well be different
	for different coordinate systems.



 History:
	04aug	ksl	52a -- Adapted from the same routine for cylindrical
			systems.
   	13sep	nsh	76b -- Changed calls to fraction to take account of
			new modes.
	15aug	ksl	Added domains.  
 
**************************************************************/



int
rtheta_where_in_grid (ndom, x)
     int ndom;
     double x[];
{
  int i, j, n;
  double r, theta;
  double f;
  int ndim, mdim;

  ndim = zdom[ndom].ndim;
  mdim = zdom[ndom].mdim;

  r = length (x);
  theta = acos ((fabs (x[2] / r))) * RADIAN;

  /* Check to see if x is outside the region of the calculation */

  if (r > zdom[ndom].wind_x[ndim - 1])  /* Fixed version */
  {
    return (-2);                /* x is outside grid */
  }
  else if (r < zdom[ndom].wind_x[0])
  {
    return (-1);                /*x is inside grid */
  }

  /* Locate the position in i and j */
  fraction (r, zdom[ndom].wind_x, ndim, &i, &f, 0);
  fraction (theta, zdom[ndom].wind_z, mdim, &j, &f, 0);

  /* Convert i,j back to n */

  wind_ij_to_n (ndom, i, j, &n);

  return (n);
}

/***********************************************************
                     Space Telescope Science Institute

 Synopsis:
 	rtheta_get_random_location

 Arguments:		
 	int n -- Cell in which random poition is to be generated
 Returns:
 	double x -- the position
 Description:	
	
		
 Notes:



 History:
	04aug	ksl	52a -- Moved from where_in_wind as incorporated
			multiple coordinate systems
	11aug	ksl	70b - Modifications to incoporate multiple 
			components
	15aug	ksl	Allow for mulitple domains
 
**************************************************************/

int
rtheta_get_random_location (n, x)
     int n;                     // Wind cell in which to create position
     double x[];                // Returned position
{
  int i, j;
  int inwind;
  double r, rmin, rmax, sthetamin, sthetamax;
  double theta, phi;
  double zz;
  int ndom, ndomain;

  ndom = wmain[n].ndom;
  wind_n_to_ij (ndom, n, &i, &j);

  rmin = zdom[ndom].wind_x[i];
  rmax = zdom[ndom].wind_x[i + 1];
  sthetamin = sin (zdom[ndom].wind_z[j] / RADIAN);
  sthetamax = sin (zdom[ndom].wind_z[j + 1] / RADIAN);

  /* Generate a position which is both in the cell and in the wind */

  inwind = W_NOT_INWIND;
  while (inwind != W_ALL_INWIND)
  {
    r = sqrt (rmin * rmin + (rand () / (MAXRAND - 0.5)) * (rmax * rmax - rmin * rmin));

    theta = asin (sthetamin + (rand () / MAXRAND) * (sthetamax - sthetamin));

    phi = 2. * PI * (rand () / MAXRAND);

/* Project from r, theta phi to x y z  */

    x[0] = r * cos (phi) * sin (theta);
    x[1] = r * sin (phi) * sin (theta);
    x[2] = r * cos (theta);
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
 	rtheta_extend_density  extends the density to
	regions just outside the wind regiions so that
	extrapolations of density can be made there

 Arguments:		
 Returns:
 Description:	
	
		
 Notes:
      Now we need to updated the densities immediately outside the wind so that the density interpolation in resonate will work.
     In this case all we have done is to copy the densities from the cell which is just in the wind (as one goes outward) to the
     cell that is just inside (or outside) the wind. 

     SS asked whether we should also be extending the wind for other parameters, especially ne.  At present we do not interpolate
     on ne so this is not necessary.  If we did do that it would be required.

     In cylindrical coordinates, the fast dimension is z; grid positions increase up in z, and then out in r.
     In spperical polar coordinates, the fast dimension is theta; the grid increases in theta (measured)
     from the z axis), and then in r.
     In spherical coordinates, the grid increases as one might expect in r..
     *
     06may      ksl     57+ -- Trying simply to use mappings to extend the grid in density space.  The
     danger is that we will do other things, e.g update some of the wrong parameters.




 History:
	05apr	ksl	56 -- Moved functionality from wind updates   
 
**************************************************************/


int
rtheta_extend_density (ndom, w)
     int ndom;
     WindPtr w;
{
  int i, j, n, m;
  int ndim, mdim;

  ndim = zdom[ndom].ndim;
  mdim = zdom[ndom].mdim;

  for (i = 0; i < ndim - 1; i++)
  {
    for (j = 0; j < mdim - 1; j++)
    {
      wind_ij_to_n (ndom, i, j, &n);
      if (w[n].vol == 0)

      {                         /*Then this grid point is not in the wind */

        wind_ij_to_n (ndom, i + 1, j, &m);
        if (w[m].vol > 0)
        {                       /*Then the windcell in the +x direction is in the wind and
                                   we can copy the densities to the grid cell n  */
          w[n].nplasma = w[m].nplasma;

        }
        else if (i > 0)
        {
          wind_ij_to_n (ndom, i - 1, j, &m);
          if (w[m].vol > 0)
          {                     /*Then the grid cell in the -x direction is in the wind and
                                   we can copy the densities to the grid cell n */
            w[n].nplasma = w[m].nplasma;

          }
        }
      }
    }
  }

  return (0);

}


/*

rtheta_is_cell_in_wind (n)

This routine performes is a robust check of whether a cell is in the wind or not.  
It was created to speed up the evaluation of the volumes for the wind.  It
checks each of the four boundaries of the wind to see whether any portions
of these are in the wind

	11aug	ksl	Added multiple components.  Note that this makes
			explicit use of the fact that we expect PART in
			a component to be 1 greater than ALL in a component,
			see python.h
	15aug	ksl	Allow for multiple domains
*/

int
rtheta_is_cell_in_wind (n)
     int n;                     /* The wind cell number */
{
  int i, j;
  double r, theta;
  double rmin, rmax, thetamin, thetamax;
  double dr, dtheta;
  double x[3];
  int ndom, mdim, ndim;
  int ndomain;



  /* First check if the cell is in the boundary */
  ndom = wmain[n].ndom;
  wind_n_to_ij (ndom, n, &i, &j);
  ndim = zdom[ndom].ndim;
  mdim = zdom[ndom].mdim;

  if (i >= (ndim - 2) && j >= (mdim - 2))
  {
    return (W_NOT_INWIND);
  }

  /* Assume that if all four corners are in the wind that the
   * entire cell is in the wind */

  if (check_corners_inwind (n) == 4)
  {
    return (W_ALL_INWIND);
  }

  /* So at this point, we have dealt with the easy cases */

  rmin = zdom[ndom].wind_x[i];
  rmax = zdom[ndom].wind_x[i + 1];
  thetamin = zdom[ndom].wind_z[j] / RADIAN;
  thetamax = zdom[ndom].wind_z[j + 1] / RADIAN;

  dr = (rmax - rmin) / RESOLUTION;
  dtheta = (thetamax - thetamin) / RESOLUTION;

  /* Check inner and outer boundary in the z direction  */

  x[1] = 0;


  for (theta = thetamin + dtheta / 2.; theta < thetamax; theta += dtheta)
  {
    x[0] = rmin * sin (theta);
    x[2] = rmin * cos (theta);;

    if (where_in_wind (x, &ndomain) == W_ALL_INWIND)
    {
      return (W_PART_INWIND);
    }

    x[0] = rmax * sin (theta);
    x[2] = rmax * cos (theta);;
    if (where_in_wind (x, &ndomain) == W_ALL_INWIND)
    {
      return (W_PART_INWIND);
    }

  }



  for (r = rmin + dr / 2.; r < rmax; r += dr)
  {
    x[0] = r * sin (thetamin);
    x[2] = r * cos (thetamin);;
    if (where_in_wind (x, &ndomain) == W_ALL_INWIND)
    {
      return (W_PART_INWIND);
    }

    x[0] = r * sin (thetamax);
    x[2] = r * cos (thetamax);;
    if (where_in_wind (x, &ndomain) == W_ALL_INWIND)
    {
      return (W_PART_INWIND);
    }

  }


  /* If one has reached this point, then this wind cell is not in the wind */
  return (W_NOT_INWIND);


}
