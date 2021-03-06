
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "atomic.h"
#include "python.h"

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis: define_phot (p, w, f1, f2, nphot_tot, ioniz_or_final, iwind, freq_sampling)
is the controlling routine for creating the underlying photon distribution.
   
Arguments:		
	PhotPtr p;
	WindPtr w;
	double f1,f2;  		The freqency mininimum and maximum if a uniform distribution 
				is selected 
	long nphot_tot;		The total number of photons that need to be generated to reach 
				the luminosity. This is not necessarily the number of photons 
				which will be generated by the call to define_phot, which instead 
				is defined by NPHOT 
	int ioniz_or_final	 0 -> this is for the wind ionization calculation, 
				1-> it is for the final spectrum calculation 
	int iwind		-1-> Do not consider wind photons under any circumstances
				0  ->Consider wind photons.  There is no need to recalculate the 
					relative contribution to number of photons from the wind
					unless the frequencies have changed
				1 -> Force a recalculation of the fraction of photons from the wind.
					This is necessary because after each ionization recalculation
					one needs to recalculate the flux fraction from the wind.
     int freq_sampling		0 --> old uniform approach, currently used for detailed spectrum calculation
				1 --> mininimum fractions ins various bins to better approximate heating
					of plasma
Returns:
 
 
Description:	

		
Notes:

History:
 	97jan   ksl	Coded and debugged as part of Python effort. 
 	98mar	ksl	Made modifications to incorporate radiation from the wind. 
 	98dec	ksl	Made mods which cause choices based on, e.g., geo.bl_radiation, to
			reflect C convention that 0 is false, non-zero is true
 	98dec	ksl	Made mods intended to assure that no matter which sources are
 			involved one always generates NPHOT photons
	01apr	ksl	Fixed problem in which phot_windstart was nonsensical if no
			wind photons.
	01aug	ksl	Modified flux fractions associated with various sources so that
			hubeny and kurucz models can be accommodated more accurately.
	01dec	ksl	Accommodated possibility of non-uniform sampling in frequency
			space. (python_40).  This involved converting photon_gen to
			a steering routine, and separating the other functions of
			photon_gen into a routine which performs initialization
			and one that generates photons.  A structure bands was
			added to store invormation about how to split the photons
			among the various energy bands, and routines init_bands and
			populate_were added to incoporate banding.  
	02jan	ksl	Modified the nature of the disk structure so now a single
			structure for entire disk (to make intepolation possible).
	04aug	ksl	Modified to reflect new spectrum type description
	13mar	ksl	Changed nphot_tot to long because it is possible
			for it to exceed the range of a normal integer
	1409	ksl	Added a loop to record the original weights of all photons
			created

**************************************************************/



// These are external variables that are used to determine whether one needs to reinitialize
// by running xdefine_phot
double f1_old = 0;
double f2_old = 0;
int iwind_old = 0;

int
define_phot (p, f1, f2, nphot_tot, ioniz_or_final, iwind, freq_sampling)
     PhotPtr p;
     double f1, f2;
     long nphot_tot;
     int ioniz_or_final;
     int iwind;
     int freq_sampling;         // 0 --> old uniform approach, 1 --> mininimum fractions ins various bins

{
  double natural_weight, weight;
  double ftot;
  int n;
  int iphot_start;


  if (freq_sampling == 0)
  {                             /* Original approach, uniform sampling of entire wavelength interval, 
                                   still used for detailed spectrum calculation */
    if (f1 != f1_old || f2 != f2_old || iwind != iwind_old)
    {                           // The reinitialization is required
      xdefine_phot (f1, f2, ioniz_or_final, iwind);
    }
    /* The weight of each photon is designed so that all of the photons add up to the
       luminosity of the photosphere.  This implies that photons must be generated in such
       a way that it mimics the energy distribution of the star. */

    geo.weight = (weight) = (geo.f_tot) / (nphot_tot);

    for (n = 0; n < NPHOT; n++)
      p[n].path = -1.0;         /* SWM - Zero photon paths */

    xmake_phot (p, f1, f2, ioniz_or_final, iwind, weight, 0, NPHOT);
  }
  else
  {                             /* Use banding, create photons with different weights in different wavelength
                                   bands.  This is used for the for ionization calculation where one wants to assure
                                   that you have "enough" photons at high energy */

    ftot = populate_bands (f1, f2, ioniz_or_final, iwind, &xband);

    for (n = 0; n < NPHOT; n++)
      p[n].path = -1.0;         /* SWM - Zero photon paths */

/* Now generate the photons */

    iphot_start = 0;
    for (n = 0; n < xband.nbands; n++)
    {

      if (xband.nphot[n] > 0)
      {
        /*Reinitialization is required here always because we are changing 
         * the frequencies around all the time */

        xdefine_phot (xband.f1[n], xband.f2[n], ioniz_or_final, iwind);

        /* The weight of each photon is designed so that all of the photons add up to the
           luminosity of the photosphere.  This implies that photons must be generated in such
           a way that it mimics the energy distribution of the star. */

        geo.weight = (natural_weight) = (ftot) / (nphot_tot);
        xband.weight[n] = weight = natural_weight * xband.nat_fraction[n] / xband.used_fraction[n];
        xmake_phot (p, xband.f1[n], xband.f2[n], ioniz_or_final, iwind, weight, iphot_start, xband.nphot[n]);

        iphot_start += xband.nphot[n];
      }
    }
  }


  for (n = 0; n < NPHOT; n++)
  {
    p[n].w_orig = p[n].w;
    p[n].freq_orig = p[n].freq;
    p[n].origin_orig = p[n].origin;
    if (geo.reverb != REV_NONE && p[n].path < 0.0)      //SWM - Set path lengths for disk, star etc. 
      simple_paths_gen_phot (&p[n]);
  }
  return (0);

}


/***********************************************************
             Space Telescope Science Institute

Synopsis: populate_bands
   
Arguments:		

Returns:
 
 
Description:	

		
Notes:

History:
	04dec	ksl	54a -- small mod to eliminate -O3 warning.

**************************************************************/


double
populate_bands (f1, f2, ioniz_or_final, iwind, band)
     double f1, f2;
     int ioniz_or_final;
     int iwind;
     struct xbands *band;

{
  double ftot, frac_used, z;
  int n, nphot, most;
  int xdefine_phot ();

// Now get all of the band limited luminosities
  ftot = 0.0;

  for (n = 0; n < band->nbands; n++)    // Now get the band limited luminosities
  {
    if (band->f1[n] < band->f2[n])
    {
      xdefine_phot (band->f1[n], band->f2[n], ioniz_or_final, iwind);
      ftot += band->flux[n] = geo.f_tot;
    }
    else
      band->flux[n] = 0.0;
    if (band->flux[n] == 0.0)
      band->min_fraction[n] = 0;        //Because you will not be able to generate photons
  }
/* So now we can distribute the photons */
  frac_used = 0;

  for (n = 0; n < band->nbands; n++)
  {
    band->nat_fraction[n] = band->flux[n] / ftot;
    frac_used += band->min_fraction[n];
  }

  nphot = 0;
  z = 0;
  most = 0;


  for (n = 0; n < band->nbands; n++)
  {
    band->used_fraction[n] = band->min_fraction[n] + (1 - frac_used) * band->nat_fraction[n];
    nphot += band->nphot[n] = NPHOT * band->used_fraction[n];
    if (band->used_fraction[n] > z)
    {
      z = band->used_fraction[n];
      most = n;
    }
  }

/* Because of roundoff errors nphot may not sum to the desired value, namely NPHOT.  So
add a few more photons to the band with most photons already. It should only be a few, at most
one photon for each band.*/

  if (nphot < NPHOT)
  {
    band->nphot[most] += (NPHOT - nphot);
  }

  return (ftot);

}


/***********************************************************
             Space Telescope Science Institute

Synopsis: xdefine_phot calculates the band limited fluxes & luminosities, and prepares
for the producting of photons (by reinitializing the disk, among other things).
   
Arguments:		

Returns:

The results are stored in the goe structure (see below).  
 
 
Description:	

This is a routine that initilizes things.  It does not generate photons itself.
		
Notes:
Note: This routine is something of a kluge.  Information is passed back to the calling
routine through the geo structure, rather than a more obvious method.  The routine was 
created when a banded approach was defined, but the whole section might be more obvious.


History:
	01dec	ksl	Broken out of the old photon_gen and simplified considerably
	04Jun   SS      Modified to allow for macro atom and kpkt generated photons in 
                        the spectrum cycles of macro atom computations
	13mar	ksl	Removed some diagnostic print statements and modified logging
			so that the frequencies being used are printed out.


**************************************************************/




int
xdefine_phot (f1, f2, ioniz_or_final, iwind)
     double f1, f2;
     int ioniz_or_final;
     int iwind;

{

  /* First determine if you need to reinitialize because the frequency boundaries are 
     different than previously */

  geo.lum_star = geo.lum_disk = 0;      // bl luminosity is an input so it is not zeroed
  geo.f_star = geo.f_disk = geo.f_bl = 0;
  geo.f_kpkt = geo.f_matom = 0; //SS - kpkt and macro atom luminosities set to zero

  if (geo.star_radiation)
  {
    star_init (geo.rstar, geo.tstar, f1, f2, ioniz_or_final, &geo.f_star);
  }
  if (geo.disk_radiation)
  {

/* Note  -- disk_init not only calculates fluxes and luminosity for the disk.  It
calculates the boundaries of the various disk annulae depending on f1 and f2 */

    disk_init (geo.rstar, geo.diskrad, geo.mstar, geo.disk_mdot, f1, f2, ioniz_or_final, &geo.f_disk);
  }
  if (geo.bl_radiation)
  {
    bl_init (geo.lum_bl, geo.t_bl, f1, f2, ioniz_or_final, &geo.f_bl);
  }
  if (geo.agn_radiation)
  {
    agn_init (geo.r_agn, geo.lum_agn, geo.alpha_agn, f1, f2, ioniz_or_final, &geo.f_agn);
  }

/* The choices associated with iwind are
iwind = -1 	Don't generate any wind photons at all
         1      Create wind photons and force a reinitialization of the wind
         0      Create wind photons but remain open to the question of whether
		the wind needs to be reinitialized.  Initialization is forced
		in that case by init
*/

  if (iwind == -1)
    geo.f_wind = geo.lum_wind = 0.0;

  if (iwind == 1 || (iwind == 0))
  {                             /* Then find the luminosity and flux of the wind */
    geo.lum_wind = wind_luminosity (0.0, VERY_BIG);
    xxxpdfwind = 1;             // Turn on the portion of the line luminosity routine which creates pdfs
    geo.f_wind = wind_luminosity (f1, f2);
    xxxpdfwind = 0;             // Turn off the portion of the line luminosity routine which creates pdfs
  }

  /* New block follow for dealing with emission via k-packets and macro atoms. SS */
  if (geo.matom_radiation)
  {
    /* JM 1408 -- only calculate macro atom emissivity if first cycle. 
       Otherwise have restarted run and can use saved emissivities */
    /* This returns the specific luminosity 
       in the spectral band of interest */
    if (geo.pcycle == 0)
    {
      geo.f_matom = get_matom_f (CALCULATE_MATOM_EMISSIVITIES);
    }
    else
      geo.f_matom = get_matom_f (USE_STORED_MATOM_EMISSIVITIES);


    geo.f_kpkt = get_kpkt_f (); /* This returns the specific luminosity 
                                   in the spectral band of interest */

    matom_emiss_report ();      // function which logs the macro atom level emissivites  
  }

  Debug ("JM: f_tot %8.2e f_star %8.2e   f_disk %8.2e   f_bl %8.2e   f_agn %8.2e f_wind %8.2e   f_matom %8.2e   f_kpkt %8.2e \n",
         geo.f_tot, geo.f_star, geo.f_disk, geo.f_bl, geo.f_agn, geo.f_wind, geo.f_matom, geo.f_kpkt);

  Log_silent
    ("xdefine_phot: lum_star %8.2e lum_disk %8.2e lum_bl %8.2e lum_agn %8.2e lum_wind %8.2e\n",
     geo.lum_star, geo.lum_disk, geo.lum_bl, geo.lum_agn, geo.lum_wind);

  Log_silent
    ("xdefine_phot:     f_star %8.2e   f_disk %8.2e   f_bl %8.2e   f_agn %8.2e f_wind %8.2e   f_matom %8.2e   f_kpkt %8.2e \n",
     geo.f_star, geo.f_disk, geo.f_bl, geo.f_agn, geo.f_wind, geo.f_matom, geo.f_kpkt);

  Log_silent
    ("xdefine_phot: wind  ff %8.2e       fb %8.2e   lines %8.2e  for freq %8.2e %8.2e\n", geo.lum_ff, geo.lum_fb, geo.lum_lines, f1, f2);

  geo.f_tot = geo.f_star + geo.f_disk + geo.f_bl + geo.f_wind + geo.f_kpkt + geo.f_matom + geo.f_agn;


/*Store the 3 variables that have to remain the same to avoid reinitialization  */

  f1_old = f1;
  f2_old = f2;
  iwind_old = iwind;
  return (0);

}


/***********************************************************
             Space Telescope Science Institute

Synopsis: xmake_phot just makes photons
   
Arguments:		

Returns:
 
 
Description:	

make_phot controls the actual generation of photons.  All of the initializations should 
have been done previously (xdefine_phot).  xmake_phot cycles throght the various possible 
sources of the wind, including for example, the disk, the central object, and the 
wind.

		
Notes:


History:
	01dec	ksl	Broken out of old photon_gen as part of major restructuring
			of how photons could be generated.  Also the calls to xmake_phot
			were made to resemble those for making photons of individual
			types
	04Jun   SS      Added options for k-packet/macro atom generated photons for
	                spectrum cycles.


**************************************************************/


int
xmake_phot (p, f1, f2, ioniz_or_final, iwind, weight, iphot_start, nphotons)
     PhotPtr p;
     double f1, f2;
     int ioniz_or_final;
     int iwind;
     double weight;
     int iphot_start;           //The place to begin putting photons in the photon structure in this call
     int nphotons;              //The total number of photons to generate in this call
{

  int nphot, nn;
  int nstar, nbl, nwind, ndisk, nmatom, nagn, nkpkt;
  double agn_f1;

/* Determine the number of photons of each type 
Error ?? -- This is a kluge.  It is intended to preserve what was done with versions earlier than
python 40 but it is not really what one wants.
*/
  nstar = nbl = nwind = ndisk = 0;
  nagn = nkpkt = nmatom = 0;

  if (geo.star_radiation)
  {
    nstar = geo.f_star / geo.f_tot * nphotons;
  }
  if (geo.bl_radiation)
  {
    nbl = geo.f_bl / geo.f_tot * nphotons;
  }
  if (iwind >= 0)
  {
    nwind = geo.f_wind / geo.f_tot * nphotons;
  }
  if (geo.disk_radiation)
  {
    ndisk = geo.f_disk / geo.f_tot * nphotons;  /* Ensure that nphot photons are created */
  }
  if (geo.agn_radiation)
  {
    nagn = geo.f_agn / geo.f_tot * nphotons;    /* Ensure that nphot photons are created */
  }
  if (geo.matom_radiation)
  {
    nkpkt = geo.f_kpkt / geo.f_tot * nphotons;
    nmatom = geo.f_matom / geo.f_tot * nphotons;
  }


  nphot = ndisk + nwind + nbl + nstar + nagn + nkpkt + nmatom;

  /* Error - This appears to be an attempt to make sure we have the right number of photons
   * but this looks as iff it could end up with the sum of the photons exceeding the number that
   * are desired.  Why isn't this a problem.  Error - ksl 101029
   */
  if (nphot < nphotons)
  {
    if (ndisk > 0)
      ndisk += (nphotons - nphot);
    else if (nwind > 0)
      nwind += (nphotons - nphot);
    else if (nbl > 0)
      nbl += (nphotons - nphot);
    else if (nagn > 0)
      nagn += (nphotons - nphot);
    else
      nstar += (nphotons - nphot);
  }


  Log
    ("photon_gen: band %6.2e to %6.2e weight %6.2e nphotons %d ndisk %6d nwind %6d nstar %6d npow %d \n",
     f1, f2, weight, nphotons, ndisk, nwind, nstar, nagn);

  /* Generate photons from the star, the bl, the wind and then from the disk */
  /* Now adding generation from kpkts and macro atoms too (SS June 04) */


  if (geo.star_radiation)
  {
    nphot = nstar;
    if (nphot > 0)
    {
      if (ioniz_or_final == 1)
        photo_gen_star (p, geo.rstar, geo.tstar, weight, f1, f2, geo.star_spectype, iphot_start, nphot);
      else
        photo_gen_star (p, geo.rstar, geo.tstar, weight, f1, f2, geo.star_ion_spectype, iphot_start, nphot);
    }
    iphot_start += nphot;
  }
  if (geo.bl_radiation)
  {
    nphot = nbl;

    if (nphot > 0)
    {
      if (ioniz_or_final == 1)
        photo_gen_star (p, geo.rstar, geo.t_bl, weight, f1, f2, geo.bl_spectype, iphot_start, nphot);
      else
        photo_gen_star (p, geo.rstar, geo.t_bl, weight, f1, f2, geo.bl_ion_spectype, iphot_start, nphot);
/* Reassign the photon type since we are actually using the same routine as for generating
stellar photons */
      nn = 0;
      while (nn < nphot)
      {
        p[iphot_start + nn].origin = PTYPE_BL;
        nn++;
      }
    }
    iphot_start += nphot;
  }

/* Generate the wind photons */

  if (iwind >= 0)
  {
    nphot = nwind;
    if (nphot > 0)
      photo_gen_wind (p, weight, f1, f2, iphot_start, nphot);
    iphot_start += nphot;
  }

/* Generate the disk photons */

  if (geo.disk_radiation)
  {
    nphot = ndisk;
    if (nphot > 0)
    {
      if (ioniz_or_final == 1)
        photo_gen_disk (p, weight, f1, f2, geo.disk_spectype, iphot_start, nphot);
      else
        photo_gen_disk (p, weight, f1, f2, geo.disk_ion_spectype, iphot_start, nphot);
    }
    iphot_start += nphot;
  }

/* Generate the agn photons */

  if (geo.agn_radiation)
  {
    nphot = nagn;
    if (nphot > 0)
    {
      /* JM 1502 -- lines to add a low frequency power law cutoff. accessible
         only in advanced mode */
      if (geo.pl_low_cutoff != 0.0 && geo.pl_low_cutoff > f1)
        agn_f1 = geo.pl_low_cutoff;

      /* error condition if user specifies power law cutoff below that hardwired in
         ionization cycles */
      else if (geo.pl_low_cutoff > f1 && ioniz_or_final == 0)
      {
        Error ("photo_gen_agn: power_law low f cutoff (%8.4e) is lower than hardwired minimum frequency (%8.4e)\n", geo.pl_low_cutoff, f1);
        agn_f1 = f1;
      }
      else
        agn_f1 = f1;


      if (ioniz_or_final == 1)
        photo_gen_agn (p, geo.r_agn, geo.alpha_agn, weight, agn_f1, f2, geo.agn_spectype, iphot_start, nphot);
      else
        photo_gen_agn (p, geo.r_agn, geo.alpha_agn, weight, agn_f1, f2, geo.agn_ion_spectype, iphot_start, nphot);
    }
    iphot_start += nphot;
  }

  /* Now do macro atoms and k-packets. SS June 04 */

  if (geo.matom_radiation)
  {
    nphot = nkpkt;
    if (nphot > 0)
    {
      if (ioniz_or_final == 0)
      {
        Error ("xmake_phot: generating photons by k-packets when performing ionization cycle. Abort.\n");
        exit (0);               //The code shouldn't be doing this - something has gone wrong somewhere. (SS June 04)
      }
      else
      {
        photo_gen_kpkt (p, weight, iphot_start, nphot);
      }
    }
    iphot_start += nphot;

    nphot = nmatom;
    if (nphot > 0)
    {
      if (ioniz_or_final == 0)
      {
        Error ("xmake_phot: generating photons by macro atoms when performing ionization cycle. Abort.\n");
        exit (0);               //The code shouldn't be doing this - something has gone wrong somewhere. (SS June 04)
      }
      else
      {
        photo_gen_matom (p, weight, iphot_start, nphot);
      }
    }
    iphot_start += nphot;
  }


  return (0);
}

/* THE NEXT FEW ROUTINES PERTAIN ONLY TO THE STAR */

/***********************************************************
              Space Telescope Science Institute

Synopsis: star_init (r, tstar, freqmin, freqmax, ioniz_or_final, f)


  
Arguments:		

Returns:

 	
Description:	

 This routine calculates the luminosity of the star and the luminosity within the frequency boundaries. 
   BB functions are assumed 
		
Notes:

History:

**************************************************************/



double
star_init (r, tstar, freqmin, freqmax, ioniz_or_final, f)
     double r, tstar, freqmin, freqmax, *f;
     int ioniz_or_final;
{
  double lumstar;
  double log_g;
  double emit, emittance_bb (), emittance_continuum ();
  int spectype;

/* 57g -- 07jul -- fixed error calculating gravity of star that has been here forever -- ksl */
  log_g = geo.gstar = log10 (G * geo.mstar / (geo.rstar * geo.rstar));
  lumstar = 4 * PI * STEFAN_BOLTZMANN * r * r * tstar * tstar * tstar * tstar;

  if (ioniz_or_final == 1)
    spectype = geo.star_spectype;       /* type for final spectrum */
  else
    spectype = geo.star_ion_spectype;   /*type for ionization calculation */

  if (spectype >= 0)
  {
    emit = emittance_continuum (spectype, freqmin, freqmax, tstar, log_g);
  }
  else
  {
    emit = emittance_bb (freqmin, freqmax, tstar);
  }

  *f = emit;                    // Calculate the surface flux between freqmin and freqmax
  *f *= (4. * PI * r * r);

  geo.lum_star = lumstar;
  return (lumstar);
}

/* Generate nphot photons from the star in the frequency interval f1 to f2 */

/***********************************************************
              Space Telescope Science Institute

Synopsis: photo_gen_star (p, r, t, weight, f1, f2, spectype, istart, nphot)


  
Arguments:		

Returns:

 	
Description:	

 This routine calculates the luminosity of the star and the luminosity within the frequency boundaries. 
   BB functions are assumed 
		
Notes:

History:
	080518	ksl	60a - Modified to use SPECTYPE_BB, SPECTYPE_UNIFORM, and 
			SPECTYPE_NONE in photon gen instead of hardcoded values

**************************************************************/

int
photo_gen_star (p, r, t, weight, f1, f2, spectype, istart, nphot)
     PhotPtr p;
     double r, t, weight;
     double f1, f2;             /* The freqency mininimum and maximum if a uniform distribution is selected */
     int spectype;              /*The spectrum type to generate: 0 is bb, 1 (or in fact anything but 0)
                                   is uniform in frequency space */
     int istart, nphot;         /* Respecitively the starting point in p and the number of photons to generate */
{
  double freqmin, freqmax, dfreq;
  int i, iend;
  if ((iend = istart + nphot) > NPHOT)
  {
    Error ("photo_gen_star: iend %d > NPHOT %d\n", iend, NPHOT);
    exit (0);
  }
  if (f2 < f1)
  {
    Error ("photo_gen_star: Cannot generate photons if freqmax %g < freqmin %g\n", f2, f1);
  }
  Log_silent ("photo_gen_star creates nphot %5d photons from %5d to %5d \n", nphot, istart, iend);
  freqmin = f1;
  freqmax = f2;
  dfreq = (freqmax - freqmin) / MAXRAND;
  r = (1. + EPSILON) * r;       /* Generate photons just outside the photosphere */
  for (i = istart; i < iend; i++)
  {
    p[i].origin = PTYPE_STAR;   // For BL photons this is corrected in photon_gen 
    p[i].w = weight;
    p[i].istat = p[i].nscat = p[i].nrscat = 0;
    p[i].grid = 0;
    p[i].tau = 0.0;
    p[i].nres = -1;             // It's a continuum photon
    p[i].nnscat = 1;

    if (spectype == SPECTYPE_BB)
    {
      p[i].freq = planck (t, freqmin, freqmax);
    }
    else if (spectype == SPECTYPE_UNIFORM)
    {                           /* Kurucz spectrum */
      /*Produce a uniform distribution of frequencies */
      p[i].freq = freqmin + rand () * dfreq;
    }
    else
    {
      p[i].freq = one_continuum (spectype, t, geo.gstar, freqmin, freqmax);
    }

    if (p[i].freq < freqmin || freqmax < p[i].freq)
    {
      Error_silent ("photo_gen_star: phot no. %d freq %g out of range %g %g\n", i, p[i].freq, freqmin, freqmax);
    }

    randvec (p[i].x, r);

    if (geo.disk_type == DISK_VERTICALLY_EXTENDED)
    {
      while (fabs (p[i].x[2]) < zdisk (r))
      {
        randvec (p[i].x, r);
      }


      if (fabs (p[i].x[2]) < zdisk (r))
      {
        Error ("Photon_gen: stellar photon %d in disk %g %g %g %g %g\n", i, p[i].x[0], p[i].x[1], p[i].x[2], zdisk (r), r);
        exit (0);
      }
    }

    randvcos (p[i].lmn, p[i].x);
  }
  return (0);
}


/* THE NEXT FEW ROUTINES PERTAIN ONLY TO THE DISK */
/***********************************************************
                           Space Telescope Science Institute

 Synopsis: disk_init calculates the total luminosity and the luminosity between freqqmin and freqmax 
	of the disk.  More importantly disk_init divides the disk into annulae such that each 
	annulus contributes and equal amount to the lumionosity of the disk (within the frequency 
	limits).  Thus disk_init initializes the structure "disk".


  
Arguments:		
	rmin,rmax		min and max radii of disk
	m			mass of central object
	mdot			mass accretion rate
	freqmin,freqmax		frequency interval of interest

Returns:

	The routine returns the total luminosity of the disk as well
	as 
	
 	*ftot			total flux in the frequency interval of 
 					interest.
 	
Description:	

		
Notes:
	The positional parameters x and v are at the edge of the ring,
	but many of the other parameters are at the mid point.

	Bug 	disk_init	This routine gets similar information throuh geo and through the call
	Bug	........	Need to adopt a consistent philosophy, and come in through one or the other

History:
 	97jan	ksl	Coded and debugged as part of Python effort. 
 	97oct8	ksl	Added velocity to the disk structure calculation
 	98mar25	ksl	Removed a line which forced disk[NMAX-1].r to
 			be the maximum radius of the disk.  This was done
 			because if one is concerned with high frequencies the disk
 			may not in fact radiate at all beyond a certain radius.  This was
 			creating problems in planck where if the minimum desired is too high
 			compared to kT/h.  But this may negatively affect the part of the
 			program which keeps track of where photons hit.
	02feb	ksl	Modified the nature of the disk structure so now a single
			structure for entire disk (to make intepolation possible).
	02feb	ksl	Assured that velocities were calculated at the same
			place as the positions so that interpolation is
			possible and also that positions were defined
			out to .r[NRINGS]
	04dec	ksl	Moved determination of spectype out of do loop, since it
			only needs to be calculated once and was genrating a -O3
			warning
	06jul	ksl	57g -- Fixed problem that we had previously been
			passing disk gravity in cgs to emittance, rather than
			log g as expected.
 

**************************************************************/


#define STEPS 100000


double
disk_init (rmin, rmax, m, mdot, freqmin, freqmax, ioniz_or_final, ftot)
     double rmin, rmax, m, mdot, freqmin, freqmax, *ftot;
     int ioniz_or_final;
{
  double t, tref, teff (), tdisk ();
  double log_g, gref, geff (), gdisk ();
  double dr, r;
  double f, ltot;
  double q1;
  int nrings;
  int spectype;
  double emit, emittance_bb (), emittance_continuum ();

  /* Calculate the reference temperature and luminosity of the disk */
  tref = tdisk (m, mdot, rmin);
  gref = gdisk (m, mdot, rmin);

  /* Now compute the apparent luminosity of the disk.  This is not actually used
     to determine how annulae are set up.  It is just used to populate geo.ltot.
     It can change if photons hitting the disk are allowed to raise the temperature
   */

  ltot = 0;
  dr = (rmax - rmin) / STEPS;
  for (r = rmin; r < rmax; r += dr)
  {
    t = teff (tref, (r + 0.5 * dr) / rmin);
    ltot += t * t * t * t * (2. * r + dr);
  }
  ltot *= 2. * STEFAN_BOLTZMANN * PI * dr;



  /* Now establish the type of spectrum to create */

  if (ioniz_or_final == 1)
    spectype = geo.disk_spectype;       /* type for final spectrum */
  else
    spectype = geo.disk_ion_spectype;   /*type for ionization calculation */

/* Next compute the band limited luminosity ftot */

/* The area of an annulus is  PI*((r+dr)**2-r**2) = PI * (2. * r +dr) * dr.  
   The extra factor of two arises because the disk radiates from both of its sides.
   */

  q1 = 2. * PI * dr;

  (*ftot) = 0;
  for (r = rmin; r < rmax; r += dr)
  {
    t = teff (tref, (r + 0.5 * dr) / rmin);
    log_g = log10 (geff (gref, (r + 0.5 * dr) / rmin));

    if (spectype > -1)
    {                           // emittance from a continuum model
      emit = emittance_continuum (spectype, freqmin, freqmax, t, log_g);
    }
    else
    {
      emit = emittance_bb (freqmin, freqmax, t);
    }

    (*ftot) += emit * (2. * r + dr);
  }

  (*ftot) *= q1;


  /* If *ftot is 0 in this energy range then all the photons come elsewhere, e. g. the star or BL  */

  if ((*ftot) < EPSILON)
  {
    Log_silent ("disk_init: Warning! Disk does not radiate enough to matter in this wavelength range\n");
    return (ltot);
  }

  /* Now find the boundaries of the each annulus, which depends on the band limited flux.
     Note that disk.v is calculated at the boundaries, because vdisk() interporlates on
     the actual radius. */

  disk.r[0] = rmin;
  disk.v[0] = sqrt (G * geo.mstar / rmin);
  nrings = 1;
  f = 0;
  for (r = rmin; r < rmax; r += dr)
  {
    t = teff (tref, (r + 0.5 * dr) / rmin);
    log_g = log10 (geff (gref, (r + 0.5 * dr) / rmin));

    if (spectype > -1)
    {                           // continuum emittance
      emit = emittance_continuum (spectype, freqmin, freqmax, t, log_g);
    }
    else
    {
      emit = emittance_bb (freqmin, freqmax, t);
    }

    f += q1 * emit * (2. * r + dr);
    /* EPSILON to assure that roundoffs don't affect result of if statement */
    if (f / (*ftot) * (NRINGS - 1) >= nrings)
    {
      if (r <= disk.r[nrings - 1])
      {
        r = disk.r[nrings - 1] * (1. + 1.e-10);
      }

      disk.r[nrings] = r;
      disk.v[nrings] = sqrt (G * geo.mstar / r);
      nrings++;
      if (nrings >= NRINGS)
      {
        Error_silent ("disk_init: Got to ftot %e at r %e < rmax %e. OK if freqs are high\n", f, r, rmax);
        break;
      }
    }
  }
  if (nrings < NRINGS - 1)
  {
    Error ("error: disk_init: Integration on setting r boundaries got %d nrings instead of %d\n", nrings, NRINGS - 1);
    exit (0);
  }


  disk.r[NRINGS - 1] = rmax;
  disk.v[NRINGS - 1] = sqrt (G * geo.mstar / rmax);


  /* Now calculate the temperature and gravity of the annulae */

  for (nrings = 0; nrings < NRINGS - 1; nrings++)
  {
    r = 0.5 * (disk.r[nrings + 1] + disk.r[nrings]);
    disk.t[nrings] = teff (tref, r / rmin);
    disk.g[nrings] = geff (gref, r / rmin);
  }

  /* Wrap up by zerrowing other parameters */
  for (nrings = 0; nrings < NRINGS; nrings++)
  {
    disk.nphot[nrings] = 0;
    disk.nhit[nrings] = 0;
    disk.heat[nrings] = 0;
    disk.ave_freq[nrings] = 0;
    disk.w[nrings] = 0;
    disk.t_hit[nrings] = 0;
  }

  geo.lum_disk = ltot;
  return (ltot);
}



/***********************************************************
                   Space Telescope Science Institute

 Synopsis: photo_gen_disk(p,weight,f1,f2,spectype,istart,nphot)

Arguments:		
	PhotPtr p;
	double weight;
	double f1,f2;   		The freqency mininimum and maximum if a uniform distribution is selected 
	int spectype;		The spectrum type to generate: 0 is bb, 1 (or in fact anything but 0)
					 is uniform in frequency space 
	int istart,nphot;   	Respectively the starting point in p and the number of photons to generate 

Returns:
 
 
Description:	


Notes:

History:
 	97jan	ksl	Coded and debugged as part of Python effort.  
 	97sep1	ksl	Added the option which allows one to construct 
			spectra from Kurucz models
	97oct8	ksl	Included Doppler shifts in Kurucz spectra of disk
	02jul	ksl	Allowed photons to be generated outside f1 and fw
			if it was due to Doppler shifts.  This was necessitated
			by narrowing the energy bands.  Note that all phtons
			produced are now doppler shifted. Eliminated Wind_ptr as
			a variable since it is not used.
	04aug	ksl	52--Modified position at which photons are generated
			to allow for a disk that is vertically extended.  Have
			not made modifications to the direction in which photons
			are emitted though this could be done.
	04dec   ksl     54d -- Minor mod to make more parallel to photo_gen_star,
	080518	ksl	60a - Modified to use SPECTYPE_BB, SPECTYPE_UNIFORM, and 
			SPECTYPE_NONE in photon gen instead of hardcoded values

**************************************************************/

int
photo_gen_disk (p, weight, f1, f2, spectype, istart, nphot)
     PhotPtr p;
     double weight;
     double f1, f2;
     int spectype;
     int istart, nphot;
{

  double freqmin, freqmax, dfreq;
  int i, iend;
  double planck ();
  double t, r, z, theta, phi;
  int nring;
  double north[3], v[3];
  if ((iend = istart + nphot) > NPHOT)
  {
    Error ("photo_gen_disk: iend %d > NPHOT %d\n", iend, NPHOT);
    exit (0);
  }
  if (f2 < f1)
  {
    Error ("photo_gen_disk: Can't do anything if f2 %g < f1 %g\n", f2, f1);
    exit (0);
  }
  Log_silent ("photo_gen_disk creates nphot %5d photons from %5d to %5d \n", nphot, istart, iend);
  freqmin = f1;
  freqmax = f2;
  dfreq = (freqmax - freqmin) / MAXRAND;
  for (i = istart; i < iend; i++)
  {
    p[i].origin = PTYPE_DISK;   // identify this as a disk photon
    p[i].w = weight;
    p[i].istat = p[i].nscat = p[i].nrscat = 0;
    p[i].tau = 0;
    p[i].nres = -1;             // It's a continuum photon
    p[i].nnscat = 1;
    if (geo.reverb_disk == REV_DISK_UNCORRELATED)
      p[i].path = 0;            //If we're assuming disk photons are uncorrelated, leave them at 0

/* The ring boundaries are defined so that an equal number of photons are
 * generated in each ring.  Howver, there is a possibility that the number
 * of photons to be generated is small, and therefore we, we still randomly
 * generate photon.  04march -- ksl
 */

    nring = ((rand () / MAXRAND) * (NRINGS - 1));

    if ((nring < 0) || (nring > NRINGS - 2))
    {
      Error ("photon_gen: photon launch out of bounds. nring = %d\n", nring);
      exit (0);
    }

    disk.nphot[nring]++;

/* The next line is really valid only if dr is small.  Otherwise one
 * should account for the area.  But haven't fixed this yet ?? 04Dec
 */

    r = disk.r[nring] + (disk.r[nring + 1] - disk.r[nring]) * rand () / MAXRAND;
    /* Generate a photon in the plane of the disk a distance r */

// This is the correct way to generate an azimuthal distribution

    phi = 2. * PI * (rand () / MAXRAND);
    p[i].x[0] = r * cos (phi);
    p[i].x[1] = r * sin (phi);

    /*04aug--ksl--Creating photons above the disk. One should actually
     * change the directionality of the photons too by modifying north */
    /*05jul -- ksl -- 56d -- Added modification to correct photon direction of a
     * vertically extended disk
     */


    z = 0.0;
    north[0] = 0;
    north[1] = 0;
    north[2] = 1;

    if (geo.disk_type == DISK_VERTICALLY_EXTENDED)
    {
      if (r == 0)
        theta = 0;
      else
      {
        z = zdisk (r);
        theta = asin ((zdisk (r * (1. + EPSILON)) - z) / (EPSILON * r));
      }
      north[0] = (-cos (phi) * sin (theta));
      north[1] = (-sin (phi) * sin (theta));
      north[2] = cos (theta);

    }

    if (rand () > MAXRAND / 2)
    {                           /* Then the photon emerges in the upper hemisphere */
      p[i].x[2] = (z + EPSILON);
    }
    else
    {
      p[i].x[2] = -(z + EPSILON);
      north[2] *= -1;
    }
    randvcos (p[i].lmn, north);

    /* Note that the next bit of code is almost duplicated in photo_gen_star.  It's
     * possilbe this should be collected into a single routine   080518 -ksl
     */

    if (spectype == SPECTYPE_BB)
    {
      t = disk.t[nring];
      p[i].freq = planck (t, freqmin, freqmax);
    }
    else if (spectype == SPECTYPE_UNIFORM)
    {                           //Produce a uniform distribution of frequencies

      p[i].freq = freqmin + rand () * dfreq;
    }

    else
    {                           /* Then we will use a model which was read in */
      p[i].freq = one_continuum (spectype, disk.t[nring], log10 (disk.g[nring]), freqmin, freqmax);
    }

    if (p[i].freq < freqmin || freqmax < p[i].freq)
    {
      Error_silent ("photo_gen_disk: phot no. %d freq %g out of range %g %g\n", i, p[i].freq, freqmin, freqmax);
    }
    /* Now Doppler shift this. Use convention of dividing when going from rest
       to moving frame */

    vdisk (p[i].x, v);
    p[i].freq /= (1. - dot (v, p[i].lmn) / C);

  }
  return (0);
}



/***********************************************************
                   Space Telescope Science Institute

 Synopsis: photo_gen_sum


Returns:
 
 
Description:	


Notes:

History:

**************************************************************/

int
phot_gen_sum (filename, mode)
     char filename[], mode[];
{
  FILE *fopen (), *ptr;
  int n;
  double x;
  if (mode[0] == 'a')
    ptr = fopen (filename, "a");
  else
    ptr = fopen (filename, "w");
  fprintf (ptr, "Ring     r      t      nphot   dN/dr\n");
  for (n = 0; n < NRINGS; n++)
  {
    if (n == 0)
      x = 0;
    else
      x = disk.nphot[n] / (disk.r[n] - disk.r[n - 1]);
    fprintf (ptr, "%d %8.2e %8.2e %8d %8.2e %8d\n", n, disk.r[n], disk.t[n], disk.nphot[n], x, disk.nhit[n]);
  }

  fclose (ptr);
  return (0);
}

/* THESE ROUTINES ARE FOR THE BOUNDARY LAYER */


/***********************************************************
                   Space Telescope Science Institute

Synopsis: bl_init (lum_bl, t_bl, freqmin, freqmax, ioniz_or_final, f)


Returns:

	The only thing that is actually calculated here is f, the luminosity
	within the frequency range that is specified.
 
 
Description:	
   This routine calculates the  luminosity of the bl within the frequency boundaries. 
   BB functions are assumed.  It was derived from the same routine for a star,
   but here we have assumed that the temperature and the luminosity are known 


Notes:
	Error ?? At present bl_init assumes a BB regardless of the spectrum.
	This is not really correct.

	0703 - ksl - This is rather an odd little routine.  As noted all that is 
	calculated is f.  ioniz_or_final is not used, and lum_bl which
	is returned is only the luminosity that was passed.

History:

**************************************************************/


double
bl_init (lum_bl, t_bl, freqmin, freqmax, ioniz_or_final, f)
     double lum_bl, t_bl, freqmin, freqmax, *f;
     int ioniz_or_final;
{
  double q1;
  double integ_planck_d ();
  double alphamin, alphamax;

  q1 = 2. * PI * (BOLTZMANN * BOLTZMANN * BOLTZMANN * BOLTZMANN) / (H * H * H * C * C);
  alphamin = H * freqmin / (BOLTZMANN * t_bl);
  alphamax = H * freqmax / (BOLTZMANN * t_bl);
  *f = q1 * integ_planck_d (alphamin, alphamax) * lum_bl / STEFAN_BOLTZMANN;
  return (lum_bl);
}
