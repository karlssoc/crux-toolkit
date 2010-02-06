/*************************************************************************//**
 * \file match_analysis.c
 * AUTHOR: Chris Park
 * CREATE DATE: Jan 03 2007
 * \brief  Given as input a directory containing binary psm files,
 * a protein database, and an optional parameter file analyze the
 * matches (with percolator or q-value) and return scores indicating
 * how good the matches are. 
 *
 * Handles at most 4 files (target and decoy).  Expects psm files to
 * end with the extension '.csm' and decoys to end with
 * '-decoy#.csm'.  Multiple target files in the given directory are
 * concatinated together and presumed to be non-overlaping parts of
 * the same ms2 file. 
 * 
 * $Revision: 1.13 $
 ****************************************************************************/
#include "q-value.h"

#define MAX_PSMS 10000000
// 14th decimal place
#define EPSILON 0.00000000000001 
#define NUM_QVALUE_OPTIONS 6
#define NUM_QVALUE_ARGUMENTS 1

/* 
 * Private function declarations.  Details below
 */

MATCH_COLLECTION_T* run_qvalue(
  char* psm_result_folder, 
  char* fasta_file 
  ); 

MATCH_COLLECTION_T* compute_bh_qvalues(
  double* pvalues, 
  int num_pvals,
  MATCH_COLLECTION_T* fasta_file);

static void print_text_files(
  MATCH_COLLECTION_T* match_collection
  );

  
/**
 * \brief One of the commands for crux.  Takes in a directory
 * containing binary psm files and a protein source (index or fasta
 * file) and calculates q-values based on the p-values calculated in
 * the search.
 */
int qvalue_main(int argc, char** argv){

  /* Define command line arguments */
  int num_options = NUM_QVALUE_OPTIONS;
  const char* option_list[NUM_QVALUE_OPTIONS] = {
    "version",
    "verbosity",
    "parameter-file",
    "overwrite",
    "output-dir",
    "fileroot"
  };

  int num_arguments = NUM_QVALUE_ARGUMENTS;
  const char* argument_list[NUM_QVALUE_ARGUMENTS] = {
    "protein input"
  };

  /* for debugging handling of parameters*/
  set_verbosity_level(CARP_ERROR);

  /* Set up parameters and set defaults in parameter.c */
  initialize_parameters();

  /* Define optional and required arguments in parameter.c */
  select_cmd_line_options(option_list, num_options );
  select_cmd_line_arguments(argument_list, num_arguments);

  /* Parse the command line and optional paramter file
     does sytnax, type, and bounds checking and dies on error */
  parse_cmd_line_into_params_hash(argc, argv, "crux compute-q-values");

  /* Get arguments */
  char* psm_dir = get_string_parameter("output-dir");
  char* protein_input_name = get_string_parameter("protein input");

  /* Get options */
  MATCH_COLLECTION_T* match_collection = NULL;

  /* Open the log file to record carp messages */
  char* log_file_name = get_string_parameter("qvalues-log-file");
  open_log_file(&log_file_name);
  free(log_file_name);
  log_command_line(argc, argv);

  carp(CARP_INFO, "Running compute q-values");

  char* param_file_name = get_string_parameter("qvalues-param-file");
  print_parameter_file(&param_file_name);
  free(param_file_name);

  /* Perform the analysis */
  match_collection = run_qvalue(psm_dir, protein_input_name);

  carp(CARP_INFO, "Outputting matches.");
  print_text_files(match_collection);

  // MEMLEAK below causes seg fault (or used to)
  // free_match_collection(match_collection);

  // clean up
  free(psm_dir);
  free(protein_input_name);

  carp(CARP_INFO, "crux calculate q-value finished.");
  exit(0);
}

/*  ****************** Subroutines ****************/

/*
 */
static void print_text_files(
  MATCH_COLLECTION_T* match_collection
  ){

  // get filename and open file
  char* out_dir = get_string_parameter("output-dir");
  char* tab_filename = get_string_parameter("qvalues-tab-output-file");
  prefix_fileroot_to_name(&tab_filename);
  BOOLEAN_T overwrite = get_boolean_parameter("overwrite");
  FILE* tab_file = create_file_in_path( tab_filename, out_dir, overwrite );

  // print header
  print_tab_header(tab_file);

  // print matches
  print_matches_multi_spectra(match_collection, tab_file, NULL);

  free(match_collection);
  free(tab_filename);
  free(out_dir);

}



/**
 * Compare doubles
 */
int compare_doubles_descending(
    const void *a,
    const void *b
    ){
  double temp = *((double *)a) - *((double *)b);
  if (temp > 0){
    return -1;
  } else if (temp < 0){
    return 1;
  } else {
    return 0;
  }
}


/**
 * \brief Compute up to three different q-values based on what is in
 * the .csm files in the directory.  Store q-values in the match
 * collection returned.
 *
 * If p-values were computed (score type LOGP_BONF_WEIBULL_XCORR), 
 * perform Benjamini-Hochberg qvalue calculations as in Klammer et
 * al. (In Press) If decoys are present (in separate files), compute
 * emperical q-values based on the number of decoys and targets above
 * the score threshold.  Use xcorr as the score to rank by. Do a
 * second calculation, ranking by p-values, if present.
 *
 * \returns a MATCH_COLLECTION object with target PSMs with at least
 * one q-value score in each match.
 */
MATCH_COLLECTION_T* run_qvalue(
  char* psm_result_folder, 
  char* fasta_file 
  ){

  // double pi0 = get_double_parameter("pi0");
  MATCH_ITERATOR_T* match_iterator = NULL;
  MATCH_COLLECTION_T* match_collection = NULL;
  MATCH_T* match = NULL;

  // array to store pvalues for bh
  const int length = MAX_PSMS;
  double* pvalues = (double*) malloc(sizeof(double) * length);
  int num_psms = 0;
  int num_pvals = 0;
  int num_decoys = 0; // to be set by match_collection_iterator

  // create MATCH_COLLECTION_ITERATOR_T object
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator =
    new_match_collection_iterator(psm_result_folder, fasta_file, &num_decoys);
  
  if( num_decoys > 1 ){
    carp(CARP_FATAL, "Only one decoy file per target can be processed "
         "but %d were found.  Please move extra decoy files.", num_decoys);
  }

  // match_collection for PSMs of all files
  MATCH_COLLECTION_T* all_matches = new_empty_match_collection(FALSE);//not decoy
  set_match_collection_scored_type(all_matches, SP, TRUE);
  set_match_collection_scored_type(all_matches, XCORR, TRUE);


  while(match_collection_iterator_has_next(match_collection_iterator)){

    // get the next match_collection
    match_collection = 
      match_collection_iterator_next(match_collection_iterator);

    // does this file contain target or decoy psms?
    BOOLEAN_T is_decoy_collection = 
      get_match_collection_is_decoy(match_collection);

    BOOLEAN_T pvalues_scored = 
      get_match_collection_scored_type(match_collection,
                                       LOGP_BONF_WEIBULL_XCORR); 
    if( pvalues_scored ){
      set_match_collection_scored_type(all_matches, 
                                       LOGP_BONF_WEIBULL_XCORR, 
                                       TRUE);
    }

    // create iterator
    match_iterator = new_match_iterator(match_collection, XCORR, FALSE);

    // gather matches into one collection, put p-value in separate array
    while(match_iterator_has_next(match_iterator)){
      match = match_iterator_next(match_iterator);

      // error if there are decoys in the target file
      if( get_match_null_peptide(match) != is_decoy_collection ){
        carp(CARP_FATAL, 
        "Cannot compute q-values from decoy PSMs in the target PSM file."
        " Run search-for-matches with --decoy-locations separate-decoy-files.");
      }

      int rank = get_match_rank(match, XCORR);

      // only use top-ranked matches
      if( rank != 1 ){
        continue;
      }

      add_match_to_match_collection(all_matches, match);

      // get p-value scores for target psms, if they exist
      if( pvalues_scored && !is_decoy_collection ){
        double cur_pval = get_match_score(match, LOGP_BONF_WEIBULL_XCORR);
        if( cur_pval != P_VALUE_NA ){
            pvalues[num_pvals++] = cur_pval;
        }
        if (num_pvals >= MAX_PSMS){
          carp(CARP_FATAL, "Too many psms in directory %s", psm_result_folder);
        }

      }// next_match
      num_psms++;

    }// next match_collection

    // ok free & update for next set
    free_match_iterator(match_iterator);
  }// next match collection

  // check that we have either pvalues or decoys
  if( num_pvals + num_decoys == 0 ){
    carp(CARP_FATAL, "Cannot compute q-values without decoy PSMs or p-values.");
  }
  carp(CARP_DEBUG, "There are %d psms for decoy qvalue computation.",
       get_match_collection_match_total(all_matches));

  free_match_collection_iterator(match_collection_iterator);

  if( num_decoys > 0 ){
    // compute decoy qvalues for xcorr
    compute_decoy_q_values(all_matches, XCORR);

    // compute decoy qvalues for pvals
    if( num_pvals > 0 ){
      compute_decoy_q_values(all_matches, LOGP_BONF_WEIBULL_XCORR);
    }
  }

  if( num_pvals > 0 ){
    compute_bh_qvalues(pvalues, num_pvals, all_matches);
  }

  free(pvalues);

  return all_matches;
}


MATCH_COLLECTION_T* compute_bh_qvalues(
  double* pvalues, 
  int num_pvals,
  MATCH_COLLECTION_T* all_matches
){

  // sort the - log p-values in descending order
  qsort(pvalues, num_pvals, sizeof(double), compare_doubles_descending);

  double* qvalues = (double*) malloc(sizeof(double) * num_pvals);

  // work in negative log space, since that is where p- and qvalues end up
  double log_num_psms = - log(num_pvals);
  double log_pi_0 = - log(get_double_parameter("pi0"));

  // convert the p-values into FDRs using Benjamini-Hochberg
  int idx;
  for (idx=0; idx < num_pvals; idx++){
    carp(CARP_DETAILED_DEBUG, "pvalue[%i] = %.10f", idx, pvalues[idx]);
    int pvalue_idx = idx + 1; // start counting pvalues at 1
    double log_pvalue = pvalues[idx];

    double log_qvalue = 
      log_pvalue + log_num_psms - (-log(pvalue_idx)) + log_pi_0;
    qvalues[idx] = log_qvalue;
    carp(CARP_DETAILED_DEBUG, "no max qvalue[%i] = %.10f", idx, qvalues[idx]);
  }

  // convert the FDRs into q-values
  double max_log_qvalue = - BILLION;
  for (idx=num_pvals-1; idx >= 0; idx--){
    if (qvalues[idx] > max_log_qvalue){
      max_log_qvalue = qvalues[idx];
    } else { // current q-value is <= max q-value
      // set to max q-value so far
      qvalues[idx] = max_log_qvalue; 
    }
    carp(CARP_DETAILED_DEBUG, "qvalue[%i] = %.10f", idx, qvalues[idx]);
  }

  // Iterate over the matches filling in the q-values
  MATCH_ITERATOR_T* match_iterator = new_match_iterator(all_matches, 
                                                        XCORR, FALSE);

  // for each match, convert p-value to q-value
  while(match_iterator_has_next(match_iterator)){
    MATCH_T* match = match_iterator_next(match_iterator);
    double log_pvalue = get_match_score(match, LOGP_BONF_WEIBULL_XCORR);
    carp(CARP_DETAILED_DEBUG, "- log pvalue  = %.6f", log_pvalue);
    
    // if p-value wasn't calculated, set q-value as nan
    if( log_pvalue == P_VALUE_NA ){
      set_match_score(match, LOGP_QVALUE_WEIBULL_XCORR, NaN() );
      continue;
    }
    
    // get the index of the p-value in the sorted list
    // FIXME slow, but it probably doesn't matter
    int idx;
    int pvalue_idx = -1;
    for (idx=0; idx < num_pvals; idx++){
      double element = pvalues[idx];
      if ((element - EPSILON <= log_pvalue) &&
          (element + EPSILON >= log_pvalue)){
        pvalue_idx = idx; // start counting pvalues at 1
        break;
      }
    }
    
    set_match_score(match, LOGP_QVALUE_WEIBULL_XCORR, qvalues[pvalue_idx]);
  }
  
  set_match_collection_scored_type(all_matches, 
                                   LOGP_QVALUE_WEIBULL_XCORR, TRUE);
  
  // free the match iterator
  free_match_iterator(match_iterator);
  free(qvalues);
  
  return all_matches;
}



/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */