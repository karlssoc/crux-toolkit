/**
 * \file spectrum.h 
 * $Revision: 1.43 $
 * \brief Object for representing one spectrum.
 *****************************************************************************/
#ifndef SPECTRUM_H
#define SPECTRUM_H

#ifdef __cplusplus
//MSToolkit Includes
#include "Spectrum.h"
#endif

//#include <stdio.h>
#include "utils.h"
#include "objects.h"
#include "peak.h"



#define MAX_I_LINES 2 // number of 'I' lines albe to parse for one spectrum object
#define MAX_D_LINES 2 // number of 'D' lines albe to parse for one spectrum object

#define _SPECTRUM_T_CLASS_ 1
#ifdef _SPECTRUM_T_CLASS_
#ifdef __cplusplus




/**
 * \class SPECTRUM_T 
 * \brief A mass spectrum

 * A mass spectrum consists mainly of a list of peak objects along with
 * some identifying information. A single spectrum is generated from one 
 * or more "scans" of the mass spectrometer; each scan is identified by 
 * a unique increasing positive integer. The range of scans that
 * generated a particular spectrum are indicated by the member variables 
 * "first_scan" and "last_scan". In addition to scan information, 
 * a tandem fragmentation mass spectrum has information 
 * about the m/z of the intact ion that generated the spectrum, which is
 * indicated by "precursor_mz" member variable.
 * Also, while the m/z of particular spectrum is known, the charge state of
 * the originating ion is unknown; the possible charge states of the 
 * precursor ion is stored "possible_z" and "num_possible_z". 
 * Finally, some summary information that can be derived from the spectrum
 * peaks but is convenient to have is stored as "min_peak_mz",
 * "max_peak_mz", and "total_energy".
 */
class SPECTRUM_T {
 protected:
  MSToolkit::Spectrum mst_spectrum;

  int              first_scan;    ///< The number of the first scan
  int              last_scan;     ///< The number of the last scan
  int              id;            ///< A unique identifier
                                  // FIXME, this field is not set when parsing
  SPECTRUM_TYPE_T  spectrum_type; ///< The type of spectrum. 
  FLOAT_T            precursor_mz;  ///< The m/z of precursor (MS-MS spectra)
  int*             possible_z;    ///< The possible charge states of this spectrum
  int              num_possible_z;///< The number of possible charge states of this spectrum

  FLOAT_T            min_peak_mz;   ///< The minimum m/z of all peaks
  FLOAT_T            max_peak_mz;   ///< The maximum m/z of all peaks
  int              num_peaks;     ///< The number of peaks
  double           total_energy;  ///< The sum of intensities in all peaks
  char*            filename;      ///< Optional filename
  char*            i_lines[MAX_I_LINES]; ///< store i lines, upto MAX_I_LINES
  char*            d_lines[MAX_D_LINES]; ///< store d lines, upto MAX_D_LINES 
  BOOLEAN_T        sorted_by_mz; ///< Are the spectrum peaks sorted by m/z...
  BOOLEAN_T        sorted_by_intensity; ///< ... or by intensity?
  BOOLEAN_T        has_mz_peak_array; ///< Is the mz_peak_array populated.

  /*
   * Parses the 'S' line of a spectrum
   * \returns TRUE if success. FALSE is failure.
   */
  BOOLEAN_T parse_S_line(
    char* line, ///< 'S' line to parse -in
    int buf_length ///< line length -in
  );

  /*
   * Parses the 'Z' line of the a spectrum
   * \returns TRUE if success. FALSE is failure.
   */
  BOOLEAN_T parse_Z_line(
    char* line  ///< 'Z' line to parse -in
  );

/*
 * Parses the 'D' line of the a spectrum
 * \returns TRUE if success. FALSE is failure.
 */
BOOLEAN_T parse_D_line(
  char* line  ///< 'D' line to parse -in
);

/*
 * Parses the 'I' line of the a spectrum
 * \returns TRUE if success. FALSE is failure.
 */
BOOLEAN_T parse_I_line(
  char* line  ///< 'I' line to parse -in
);

/*
 * Adds a possible charge(z) to the spectrum.
 * Must not exceed the MAX_CHARGE capacity
 */
BOOLEAN_T add_possible_z(
  int charge  ///< charge to add
);

 void populate_mz_peak_array();



 public:
 //TODO : Protect these!
  BOOLEAN_T        has_peaks;  ///< Does the spectrum contain peak information
  PEAK_T**         mz_peak_array;  ///< Allows rapid peak retrieval by mz.
  PEAK_T*          peaks;         ///< The spectrum peaks


  void init();
  SPECTRUM_T(); //allocate_spectrum
  SPECTRUM_T(
    int               first_scan,         ///< The number of the first scan -in
    int               last_scan,          ///< The number of the last scan -in
    SPECTRUM_TYPE_T   spectrum_type,      ///< The type of spectrum. -in
    FLOAT_T             precursor_mz,       ///< The m/z of the precursor (for MS-MS spectra) -in
    int*              possible_z,         ///< The possible charge states of this spectrum  -in
    int               num_possible_z,     ///< The number of possible charge states of this spectrum  -in  
    char*             filename);          ///< Optional filename  -in    
    
 ~SPECTRUM_T(); // free_spectrum
 void print(FILE* file);
 void print_sqt(FILE* file,///< output file to print at -out
  int num_matches,      ///< number of peptides compared to this spec -in
  int charge            ///< charge used for the search -in
  ); 
  
 void print_stdout();

 static void copy(
   SPECTRUM_T* src, ///< the source spectrum -in
   SPECTRUM_T* dest ///< the destination spectrum -out
   );

 BOOLEAN_T parse_spectrum_file(
  FILE* file, ///< the input file stream -in
  char* filename ///< filename of the spectrum, should not free -in
  );

 BOOLEAN_T parse_spectrum(
  char*      filename ///< the file to parse -in
  );

 static SPECTRUM_T* parse_spectrum_binary(
   FILE* file ///< output stream -out
  );
  
 void sum_normalize_spectrum();
 void spectrum_rank_peaks();
 int get_first_scan();
 void set_first_scan(int first_scan);
 int get_last_scan();
 void set_last_scan(int last_scan);

 int get_id();
 void set_id(int id);
 
 SPECTRUM_TYPE_T get_spectrum_type();
 
 void set_spectrum_type(SPECTRUM_TYPE_T);


 FLOAT_T get_precursor_mz();
 void set_precursor_mz(FLOAT_T precursor_mz);
 
 

 int* get_possible_z();
 
 int* get_possible_z_pointer();
 
 int get_charged_to_search(int**);
 void set_possible_z(int* possible_z, int num_possible_z);
 
 void set_new_possible_z(
   int* possible_z, ///< possible z array -in
   int num_possible_z ///< possible z array size -in
 );		 

 int get_num_possible_z();

 FLOAT_T get_min_peak_mz();

 FLOAT_T get_max_peak_mz();
 
 int get_num_peaks();

 double get_total_energy();

 char* get_filename();

 void set_filename(char* filename);

 void set_new_filename(char* filename);
 
 FLOAT_T get_max_peak_intensity();

 FLOAT_T get_mass(int charge);
 
 FLOAT_T get_neutral_mass(int charge);
 
 FLOAT_T get_singly_charged_mass(int charge);

 void update_spectrum_fields(
   FLOAT_T intensity, ///< the intensity of the peak that has been added -in
   FLOAT_T location ///< the location of the peak that has been added -in
   );

 BOOLEAN_T add_peak(
  FLOAT_T intensity, ///< the intensity of peak to add -in
  FLOAT_T location_mz ///< the location of peak to add -in
  );
 PEAK_T* get_nearest_peak(
  FLOAT_T mz, ///< the mz of the peak around which to sum intensities -in
  FLOAT_T max ///< the maximum distance to get intensity -in
  );
 
 FLOAT_T get_nearby_intensity_sum( 
  FLOAT_T mz,             ///< the mz of the peak around which to sum intensities
  FLOAT_T tol             ///< the tolerance within which to sum intensities
  );

 
/**
 * process the spectrum, according the score type
 *\returns a new spectrum that has been preprocessed
 */
SPECTRUM_T* process_spectrum(
  SPECTRUM_T* spectrum, ///< the spectrum to processes -in
  SCORER_TYPE_T score_type ///< the score type to which the spectrum should be sorted -in
  );

/**
 * serialize the spectrum in binary
 * Form,
 * <int: first_scan><int: last_scan><int: id><SPECTRUM_TYPE_T: spectrum_type>
 * <float: precursor_mz><float: retention_time>
 */
void serialize(
  FILE* file ///< output stream -out
  );

/***********************************************************************
 * Normalize peak intensities so that they sum to unity.
 ***********************************************************************/
void sum_normalize_spectrum(
  SPECTRUM_T* spectrum
  );

/***********************************************************************
 * Populate peaks with rank information.
 ***********************************************************************/
void rank_peaks(
  SPECTRUM_T* spectrum
  );

 int get_charges_to_search(int** select_charge_array);  

};
#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif
#ifndef _SPECTRUM_T_CLASS_
/**
 * \returns An (empty) spectrum object.
 */
SPECTRUM_T* allocate_spectrum(void);

/**
 * \returns A new spectrum object, populated with the user specified parameters.
 */
SPECTRUM_T* new_spectrum(
  int               first_scan,         ///< The number of the first scan -in
  int               last_scan,          ///< The number of the last scan -in
  SPECTRUM_TYPE_T   spectrum_type,      ///< The type of spectrum. -in
  FLOAT_T             precursor_mz,       ///< The m/z of the precursor (for MS-MS spectra) -in
  int*              possible_z,         ///< The possible charge states of this spectrum  -in
  int               num_possible_z,     ///< The number of possible charge states of this spectrum  -in  
  char*             filename);          ///< Optional filename  -in

/**
 * Frees an allocated spectrum object.
 */
void free_spectrum (
  SPECTRUM_T* spectrum ///< the spectrum to free -in
  );

/**
 * Prints a spectrum object to file.
 */
void print_spectrum(
  SPECTRUM_T* spectrum, ///< spectrum to print -in
  FILE* file ///< output file to print at -out
  );

/**
 * Prints a spectrum object to file in sqt format.
 */
void print_spectrum_sqt(
  SPECTRUM_T* spectrum, ///< spectrum to print -in
  FILE* file,           ///< output file to print at -out
  int num_matches,      ///< number of peptides compared to this spec -in
  int charge            ///< charge used for the search -in
  );

/**
 * Prints a spectrum object to STDOUT.
 */
void print_spectrum_stdout(
  SPECTRUM_T* spectrum ///< the spectrum to print -in
  );

/**
 * Copies spectrum object src to dest.
 * must pass in a memory allocated SPECTRUM_T* dest
 * doesn't copy the sum array related fields
 */
void copy_spectrum(
  SPECTRUM_T* src, ///< the source spectrum -in
  SPECTRUM_T* dest ///< the destination spectrum -out
  );

/**
 * Parses a spectrum from file.
 * \returns TRUE if success. FALSE is failure.
 * 'I'
 * Skips Header line "H"
 * FIXME if need to read 'H', header line, does not parse ID
 */
BOOLEAN_T parse_spectrum_file(
  SPECTRUM_T* spectrum, ///< spectrum to parse the information into -out
  FILE* file, ///< the input file stream -in
  char* filename ///< filename of the spectrum, should not free -in
  );

/**
 * \returns TRUE if success. FALSE if failure.
 */
BOOLEAN_T parse_spectrum(
  SPECTRUM_T* spectrum, ///< spectrum to parse the information into -out
  char*      filename ///< the file to parse -in
  );

/**
 * Parse the spectrum from the serialized spectrum
 *\returns the parsed spectrum , else returns NULL for failed parse
 */
SPECTRUM_T* parse_spectrum_binary(
  FILE* file ///< output stream -out
  );

/***********************************************************************
 * Normalize peak intensities so that they sum to unity.
 ***********************************************************************/
void sum_normalize_spectrum(
  SPECTRUM_T* spectrum
  );

/***********************************************************************
 * Populate peaks with rank information.
 ***********************************************************************/
void spectrum_rank_peaks(
  SPECTRUM_T* spectrum
  );

/** 
 * Access routines of the form get_<object>_<field> and set_<object>_<field>. 
 */

/**
 * Additional get and set methods
 */

/**
 * \returns the number of the first scan
 */
int get_spectrum_first_scan(
  SPECTRUM_T* spectrum ///< the spectrum to query the first scan -in
  );

/**
 * \sets the number of the first scan
 */
void set_spectrum_first_scan(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the first scan -out
  int first_scan ///< the first_scan -in
  );

/**
 * \returns the number of the last scan
 */
int get_spectrum_last_scan(
  SPECTRUM_T* spectrum  ///< the spectrum to query the last scan -in
  );

/**
 * \sets the number of the last scan
 */
void set_spectrum_last_scan(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the last scan -out
  int last_scan ///< the last scan -in
  );

/**
 * \returns the spectrum_id
 */
int get_spectrum_id(
  SPECTRUM_T* spectrum  ///< the spectrum to query the id -in
  );

/**
 * \sets the spectrum_id
 */
void set_spectrum_id(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the id -out
  int id ///< the id -in
  );

/**
 * \returns the spectrum_type
 */
SPECTRUM_TYPE_T get_spectrum_spectrum_type(
  SPECTRUM_T* spectrum  ///< the spectrum to query the spectrum_type -in
  );

/**
 * \sets the spectrum_type
 */
void set_spectrum_spectrum_type(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the spectrum_type -out
  SPECTRUM_TYPE_T spectrum_type ///< the spectrum type -in
  );

/**
 * \returns the m/z of the precursor
 */
FLOAT_T get_spectrum_precursor_mz(
  SPECTRUM_T* spectrum  ///< the spectrum to query the precursor_mz -in
  );

/**
 * \sets the m/z of the precursor
 */
void set_spectrum_precursor_mz(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the precursor_mz -out
  FLOAT_T precursor_mz ///< the precursor_mz -in
  );

/**
 * \returns the possible charge states of this spectrum
 * returns an int* to a heap allocated copy of the src spectrum
 * thus, the user must free the memory
 * number of possible charge states can be gained by 
 * the get_num_possible_z function.
 */
int* get_spectrum_possible_z(
  SPECTRUM_T* spectrum  ///< the spectrum to query possible z -in
  );

/**
 * \returns the number of possible charge states of this spectrum
 */
int get_num_possible_z(
  SPECTRUM_T* spectrum  ///< the spectrum to query possible z -in
  );

/**
 * \returns a pointer to an array of the possible charge states of this spectrum
 * User must NOT free this or alter, not a copy
 * number of possible charge states can be gained by 
 * the get_num_possible_z function.
 */
int* get_spectrum_possible_z_pointer(
  SPECTRUM_T* spectrum  ///< the spectrum to query possible z -in
  );
 
int get_charges_to_search(SPECTRUM_T*, int**);
/**
 * \sets the possible charge states of this spectrum
 * the function copies the possible_z into a heap allocated memory
 * num_possible_z must match the array size of possible_z 
 * frees the memory of the possible_z that is replaced
 * updates the number of possible charge states field
 */
void set_spectrum_possible_z(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the new_possible_z -out
  int* possible_z, ///< possible z array -in
  int num_possible_z ///< possible z array size -in
  );


/**
 * \sets the possible charge states of this spectrum
 * this function should only be used when possible_z is set to NULL
 * to change existing possible_z use set_spectrum_possible_z()
 * the function copies the possible_z into a heap allocated memory
 * num_possible_z must match the array size of possible_z 
 * updates the number of possible charge states field
 */
void set_spectrum_new_possible_z(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the new_possible_z -out
  int* possible_z, ///< possible z array -in
  int num_possible_z ///< possible z array size -in
  );

/**
 * \returns the number of possible charge states of this spectrum
 */
int get_spectrum_num_possible_z(
  SPECTRUM_T* spectrum  ///< the spectrum to query length of possible_z array -in
  );

/**
 * \returns the minimum m/z of all peaks
 */
FLOAT_T get_spectrum_min_peak_mz(
  SPECTRUM_T* spectrum ///< the spectrum to query min_peak_mz -in
  );

/**
 * \returns the maximum m/z of all peaks
 */
FLOAT_T get_spectrum_max_peak_mz(
  SPECTRUM_T* spectrum  ///< the spectrum to query max_peak_mz -in
  );

/**
 * \returns the number of peaks
 */
int get_spectrum_num_peaks(
  SPECTRUM_T* spectrum  ///< the spectrum to query number of peaks -in
  );

/**
 * \returns the sum of intensities in all peaks
 */
double get_spectrum_total_energy(
  SPECTRUM_T* spectrum  ///< the spectrum to query total energy -in
  );


/**
 * \returns the filename of the ms2 file the spectrum was parsed
 * returns a char* to a heap allocated copy of the filename
 * user must free the memory
 */
char* get_spectrum_filename(
  SPECTRUM_T* spectrum  ///< the spectrum to query filename -in
  );

/**
 * \sets the filename of the ms2 file the spectrum was parsed
 * copies the value from arguement char* filename into a heap allocated memory
 * frees memory for the filename that is replaced
 */
void set_spectrum_filename(
  SPECTRUM_T* spectrum,  ///< the spectrum to set the filename -out
  char* filename ///< the filename -in
  );

/**
 * \sets the filename of the ms2 file the spectrum was parsed
 * this function should be used only the first time the filename is set(set to NULL)
 * to change existing filename use set_spectrum_filename
 * copies the value from arguement char* filename into a heap allocated memory
 */
void set_spectrum_new_filename(
  SPECTRUM_T* spectrum,   ///< the spectrum to set the filename -out
  char* filename ///< the filename -in
  );

/**
 * \returns The intensity of the peak with the maximum intensity.
 */
FLOAT_T get_spectrum_max_peak_intensity(
  SPECTRUM_T* spectrum  ///< the spectrum to query maximum peak intensity -in
  );

/**
 * \returns The mass of the charged precursor ion, according to the formula 
 * mass = m/z * charge
 */
FLOAT_T get_spectrum_mass(
  SPECTRUM_T* spectrum,  ///< the spectrum to query spectrum mass -in
  int charge ///< the charge of precursor ion -in
  );

/**
 * \returns The mass of the neutral precursor ion, according to the formula 
 * mass = m/z * charge - mass_H * charge
 */
FLOAT_T get_spectrum_neutral_mass(
  SPECTRUM_T* spectrum,  ///< the spectrum to query neutral_mass -in
  int charge ///< the charge of precursor ion -in
  );

/**
 * \returns The mass of the singly charged precursor ion, according to the formula 
 * mass = m/z * charge - (mass_H * (charge - 1))
 */
FLOAT_T get_spectrum_singly_charged_mass(
  SPECTRUM_T* spectrum,  ///< the spectrum to query charged_mass -in
  int charge ///< the charge of the precursor ion -in
  );

/**
 * updates num_peaks, min_peak_mz, max_peak_mz, total_energy fields in spectrum
 */
void update_spectrum_fields(
  SPECTRUM_T* spectrum, ///< the spectrum fields to update -out
  FLOAT_T intensity, ///< the intensity of the peak that has been added -in
  FLOAT_T location ///< the location of the peak that has been added -in
  );


/*
 * Adds a possible charge(z) to the spectrum.
 * Must not exceed the MAX_CHARGE capacity
 */
BOOLEAN_T add_possible_z(
  SPECTRUM_T* spectrum,  ///< place Z line into this spectrum -out   
  int charge  ///< charge to add
);

/**
 * Adds a peak to the spectrum given a intensity and location
 * calls update_spectrum_fields to update num_peaks, min_peak ...
 */
BOOLEAN_T add_peak_to_spectrum(
  SPECTRUM_T* spectrum,///< spectrum to add the peak to -out 
  FLOAT_T intensity, ///< the intensity of peak to add -in
  FLOAT_T location_mz ///< the location of peak to add -in
  );

/**
 * \returns The closest PEAK_T within 'max' of 'mz' in 'spectrum'
 * NULL if no peak within 'max'
 * This should lazily create the data structures within the
 * spectrum object that it needs.
 */
PEAK_T* get_nearest_peak(
  SPECTRUM_T* spectrum, ///< the spectrum to query the intensity sum -in
  FLOAT_T mz, ///< the mz of the peak around which to sum intensities -in
  FLOAT_T max ///< the maximum distance to get intensity -in
  );

/**
 * \returns The sum of intensities within 'tolerance' of 'mz' in 'spectrum'
 * This should lazily create the data structures within the
 * spectrum object that it needs.
 * NOTE: Not implemented!
 */
FLOAT_T get_nearby_intensity_sum(
  SPECTRUM_T* spectrum, ///< the spectrum to query the intensity sum -in
  FLOAT_T mz,             ///< the mz of the peak around which to sum intensities
  FLOAT_T tol             ///< the tolerance within which to sum intensities
  );

/**
 * process the spectrum, according the score type
 *\returns a new spectrum that has been preprocessed
 */
SPECTRUM_T* process_spectrum(
  SPECTRUM_T* spectrum, ///< the spectrum to processes -in
  SCORER_TYPE_T score_type ///< the score type to which the spectrum should be sorted -in
  );

/**
 * serialize the spectrum in binary
 * Form,
 * <int: first_scan><int: last_scan><int: id><SPECTRUM_TYPE_T: spectrum_type>
 * <float: precursor_mz><float: retention_time>
 */
void serialize_spectrum(
  SPECTRUM_T* spectrum, ///< the spectrum to serialize -in
  FILE* file ///< output stream -out
  );

/***********************************************************************
 * Normalize peak intensities so that they sum to unity.
 ***********************************************************************/
void sum_normalize_spectrum(
  SPECTRUM_T* spectrum
  );

/***********************************************************************
 * Populate peaks with rank information.
 ***********************************************************************/
void rank_peaks(
  SPECTRUM_T* spectrum
  );

#endif

/******************************************************************************/

/**
 * Instantiates a new peak_iterator from a spectrum.
 * \returns a PEAK_ITERATOR_T object.
 */
PEAK_ITERATOR_T* new_peak_iterator(
  SPECTRUM_T* spectrum ///< the spectrum peaks to iterate -in
  );

/**
 * Frees an allocated peak_iterator object.
 */
void free_peak_iterator(
  PEAK_ITERATOR_T* peak_iterator ///< the interator to free -in
  );

/**
 * The basic iterator functions. 
 * \returns TRUE if there are additional peaks to iterate over, FALSE if not.
 */
BOOLEAN_T peak_iterator_has_next(
  PEAK_ITERATOR_T* peak_iterator ///< the interator for the peaks -in
  );

/**
 * \returns The next peak object in the spectrum, in order of m/z.
 */
PEAK_T* peak_iterator_next(
  PEAK_ITERATOR_T* peak_iterator  ///< the interator for the peaks -in
  );

/**
 *  Resets the iterator to the first element
 */
void peak_iterator_reset(
  PEAK_ITERATOR_T* peak_iterator  ///< the interator for the peaks -in
);

/**
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */


#ifdef __cplusplus
}
#endif

#endif

/** \mainpage The crux API documentation page.
 * \section Introduction
 * Welcome to crux, a C software package for analysis of tandem mass
 * spectrometry data. Click on the links above to see documentation for
 * crux objects and their user interfaces.
 */
