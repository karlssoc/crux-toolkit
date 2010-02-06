/*********************************************************************//**
 * \file match_collection.c
 * \brief A set of peptide spectrum matches for one spectrum.
 *
 * Methods for creating and manipulating match_collections.   
 * Creating a match collection generates all matches (searches a
 * spectrum against a database.
 *
 * AUTHOR: Chris Park
 * CREATE DATE: 11/27 2006
 * $Revision: 1.123 $
 ****************************************************************************/
#include "match_collection.h"

/* Private data types (structs) */

/**
 * \struct match_collection
 * \brief An object that contains a set of match objects.
 *
 * May contain matches for one spectrum or many spectra. 
 * 
 * 
 */
struct match_collection{
  MATCH_T* match[_MAX_NUMBER_PEPTIDES]; ///< array of match object
  int match_total;      ///< size of match array, may vary w/truncation
  int experiment_size;  ///< total matches before any truncation
  // TODO this should be removed, stored in match
  int charge;           ///< charge of the associated spectrum
  BOOLEAN_T null_peptide_collection; ///< are the peptides shuffled
  BOOLEAN_T scored_type[_SCORE_TYPE_NUM]; 
                        ///< TRUE if matches have been scored by the type
  SCORER_TYPE_T last_sorted; 
    ///< the last type by which it's been sorted ( -1 if unsorted)
  BOOLEAN_T iterator_lock; 
    ///< has an itterator been created? if TRUE can't manipulate matches

  // values used for various scoring functions.
  // TODO this should be moved to match
  FLOAT_T delta_cn; ///< the difference in top and second Xcorr scores
  FLOAT_T sp_scores_sum; ///< for getting mean, backward compatible
  FLOAT_T sp_scores_mean;  ///< the mean value of the scored peptides sp score
  FLOAT_T mu;// obsolete 
  ///< EVD parameter Xcorr(characteristic value of extreme value distribution)
  FLOAT_T l_value; // obsolete
  ///< EVD parameter Xcorr(decay constant of extreme value distribution)
  int top_fit_sp; // obsolete
  ///< The top ranked sp scored peptides to use as EXP_SP parameter estimation
  FLOAT_T base_score_sp; // obsolete
 ///< The lowest sp score within top_fit_sp, used as the base to rescale sp
  // Values for fitting the Weibull distribution
  FLOAT_T eta;  ///< The eta parameter for the Weibull distribution.
  FLOAT_T beta; ///< The beta parameter for the Weibull distribution.
  FLOAT_T shift; ///< The location parameter for the Weibull distribution.
  FLOAT_T correlation; ///< The correlation parameter for the Weibull distribution.
  // replace this ...
  MATCH_T* sample_matches[_PSM_SAMPLE_SIZE];
  int num_samples;  // the number of items in the above array
  // ...with this
  FLOAT_T xcorrs[_MAX_NUMBER_PEPTIDES]; ///< xcorrs to be used for weibull
  int num_xcorrs;

  // The following features (post_*) are only valid when
  // post_process_collection boolean is TRUE 
  BOOLEAN_T post_process_collection; 
  ///< Is this a post process match_collection?
  int post_protein_counter_size; 
  ///< the size of the protein counter array, usually the number of proteins in database
  int* post_protein_counter; 
  ///< the counter for how many each protein has matches other PSMs
  int* post_protein_peptide_counter; 
  ///< the counter for how many each unique peptides each protein has matches other PSMs
  HASH_T* post_hash; ///< hash table that keeps tracks of the peptides
  BOOLEAN_T post_scored_type_set; 
  ///< has the scored type been confirmed for the match collection,
  // set after the first match collection is extended
};

/**
 *\struct match_iterator
 *\brief An object that iterates over the match objects in the
 * specified match_collection for the specified score type (SP, XCORR)
 */
struct match_iterator{
  MATCH_COLLECTION_T* match_collection; 
                            ///< the match collection to iterate -out
  SCORER_TYPE_T match_mode; ///< the current working score (SP, XCORR)
  int match_idx;            ///< current match to return
  int match_total;          ///< total_match_count
};

/**
 * \struct match_collection_iterator
 * \brief An object that iterates over the match_collection objects in
 * the specified directory of serialized match_collections 
 */
struct match_collection_iterator{
  DIR* working_directory; 
  ///< the working directory for the iterator to find match_collections
  char* directory_name; ///< the directory name in char
  DATABASE_T* database; ///< the database for which the match_collection
  int number_collections; 
  ///< the total number of match_collections in the directory (target+decoy)
  int collection_idx;  ///< the index of the current collection to return
  MATCH_COLLECTION_T* match_collection; ///< the match collection to return
  BOOLEAN_T is_another_collection; 
  ///< is there another match_collection to return?
};

/******* Private function declarations, described in definintions below ***/

BOOLEAN_T score_peptides(
  SCORER_TYPE_T score_type, 
  MATCH_COLLECTION_T* match_collection, 
  SPECTRUM_T* spectrum, 
  int charge, 
  MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator,
  BOOLEAN_T is_decoy
);

BOOLEAN_T add_unscored_peptides(
  MATCH_COLLECTION_T* match_collection, 
  SPECTRUM_T* spectrum, 
  int charge, 
  MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator,
  BOOLEAN_T is_decoy
);

BOOLEAN_T score_matches_one_spectrum(
  SCORER_TYPE_T score_type, 
  MATCH_T** matches,
  int num_matches,
  SPECTRUM_T* spectrum,
  int charge
  );

BOOLEAN_T populate_match_rank_match_collection(
 MATCH_COLLECTION_T* match_collection, 
 SCORER_TYPE_T score_type 
 );

void store_new_xcorrs(MATCH_COLLECTION_T* match_collection, 
                      int start_index,
                      BOOLEAN_T keep_matches);

void collapse_redundant_matches(MATCH_COLLECTION_T* matches);
void consolidate_matches(MATCH_T** matches, int start_idx, int end_idx);

BOOLEAN_T extend_match_collection(
  MATCH_COLLECTION_T* match_collection, 
  DATABASE_T* database, 
  FILE* result_file   
  );

BOOLEAN_T add_match_to_post_match_collection(
  MATCH_COLLECTION_T* match_collection, 
  MATCH_T* match 
  );

void update_protein_counters(
  MATCH_COLLECTION_T* match_collection, 
  PEPTIDE_T* peptide  
  );

BOOLEAN_T calculate_delta_cn(
  MATCH_COLLECTION_T* match_collection);

/********* end of function declarations *******************/


/**
 * \returns An (empty) match_collection object.
 */
MATCH_COLLECTION_T* allocate_match_collection()
{
  MATCH_COLLECTION_T* match_collection =
    (MATCH_COLLECTION_T*)mycalloc(1, sizeof(MATCH_COLLECTION_T));
    
  // loop over to set all score type to FALSE
  int score_type_idx = 0;
  for(; score_type_idx < _SCORE_TYPE_NUM; ++score_type_idx){
    match_collection->scored_type[score_type_idx] = FALSE;
  }
  
  // set last score to -1, thus nothing has been done yet
  match_collection->last_sorted = -1;
  match_collection->iterator_lock = FALSE;
  match_collection->post_process_collection = FALSE;
  match_collection->null_peptide_collection = FALSE;
  
  return match_collection;
}

/**
 * /brief Free the memory allocated for a match collection
 * Deep free; each match is freed which, in turn, frees each spectrum
 * and peptide. 
 */
void free_match_collection(
  MATCH_COLLECTION_T* match_collection ///< the match collection to free -out
  )
{
  // decrement the pointer count in each match object
  while(match_collection->match_total > 0){
    --match_collection->match_total;
    free_match(match_collection->match[match_collection->match_total]);
    match_collection->match[match_collection->match_total] = NULL;
  }

  // and free the sample matches
  while(match_collection->num_samples > 0){
    --match_collection->num_samples;
   free_match(match_collection->sample_matches[match_collection->num_samples]);
    match_collection->sample_matches[match_collection->num_samples] = NULL;
  }
  
  // free post_process_collection specific memory
  if(match_collection->post_process_collection){
    // free protein counter
    free(match_collection->post_protein_counter);
    
    // free protein peptide counter
    free(match_collection->post_protein_peptide_counter);
  
    // free hash table
    free_hash(match_collection->post_hash);
  }


  free(match_collection);
}


/**
 * \brief Creates a new match collection with no matches in it.  Sets
 * the member variable indicating if the matches are to real peptides
 * or to decoy (shuffled) peptides. Other member variables are set to
 * default values.  The method add_matches() can be used to search a
 * spectrum and store the matches in this collection.  
 *
 * \returns A newly allocated match collection
 */
MATCH_COLLECTION_T* new_empty_match_collection(BOOLEAN_T is_decoy){

  MATCH_COLLECTION_T* match_collection = allocate_match_collection();
  int idx = 0;  

  // set member variables according to parameter.c 
  match_collection->match_total = 0;
  match_collection->experiment_size = 0; 
  match_collection->charge = 0;
  match_collection->null_peptide_collection = is_decoy;

  for(idx=0; idx<_SCORE_TYPE_NUM; idx++){
    match_collection->scored_type[idx] = FALSE;
  }
  match_collection->last_sorted = -1;
  match_collection->iterator_lock = FALSE;
  match_collection->num_samples = 0;
  match_collection->num_xcorrs = 0;

  match_collection->post_hash = NULL;

  return match_collection;
}

/**
 * \brief The main search function.  All peptides in the peptide
 * iterator are compared to the spectrum and the resulting score(s)
 * are stored in a match.  All matches are stored in the
 * match_collection.  Can be called on an empty match_collection or
 * one already containing matches.  No checks to confirm that the same
 * spectrum is being searched in subsequent calls.
 *
 * First, the preliminary score (as in parameter.c) is used to compare
 * peptides and spectrum.  These results are then sorted and the final
 * score (as in parameter.c) is calculated on the top-match
 * (parameter.c) top matches as ranked by the preliminary score.  No
 * matches are deleted after ranking.
 *
 * When called on a match collection already containing matches, the
 * preliminary score is calculated for all new peptides.  All matches
 * (from this peptide iterator and previous) are sorted by prelim
 * score and only the top-match matches are scored for the final
 * score.  Previously scored matches are not scored twice.
 *
 * \returns The number of matches added.
 */
int add_matches(
  MATCH_COLLECTION_T* match_collection,///< add matches to this
  SPECTRUM_T* spectrum,  ///< compare peptides to this spectrum
  int charge,            ///< use this charge state for spectrum
  MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator, ///< use these peptides
  BOOLEAN_T is_decoy,     ///< are peptides to be shuffled
  //BF: this was added so that a match_collection could be mixed target/decoy
  BOOLEAN_T keep_matches  ///< FALSE=only save xcorr for p-val estimation
){
  if( match_collection == NULL || peptide_iterator == NULL
      || spectrum == NULL ){
    carp(CARP_FATAL, "Cannot add matches to a collection when match " \
         "collection, spectrum and/or peptide iterator are NULL.");
  }

  // charge==0 if collection has no matches yet
  assert(match_collection->charge==0 || match_collection->charge==charge);
  match_collection->charge = charge;
  match_collection->last_sorted = -1;

  int num_matches_added = 0;
  int start_index = match_collection->match_total;

  // TODO (BF 16-mar-09): change to add_unscored_peptides, 
  // then score_matches_one_spectrum with one or both scores

  // preliminary scoring
  int sp_max_rank = get_int_parameter("max-rank-preliminary");
  SCORER_TYPE_T prelim_score = get_scorer_type_parameter("prelim-score-type");

  if( sp_max_rank == 0 ){ 
    add_unscored_peptides(match_collection, spectrum, charge, 
                          peptide_iterator, is_decoy);
  }else{
    score_peptides(prelim_score, match_collection, spectrum, 
                   charge, peptide_iterator, is_decoy);
  }
  num_matches_added = match_collection->match_total - start_index;

  // score existing matches w/second function
  SCORER_TYPE_T final_score = get_scorer_type_parameter("score-type");
  score_matches_one_spectrum(final_score, match_collection->match,
                             match_collection->match_total, spectrum, charge);

  match_collection->scored_type[final_score] = TRUE;

  // store xcorrs from newly-score psms
  store_new_xcorrs(match_collection, start_index, keep_matches); // replaces the sample step

  if( sp_max_rank > 0 ){ 
    // rank by sp first 
    populate_match_rank_match_collection(match_collection, prelim_score);
    truncate_match_collection( match_collection, sp_max_rank, prelim_score);
  }

  // rank by xcorr
  populate_match_rank_match_collection(match_collection, final_score);

  if( sp_max_rank == 0 ){ // truncate here if not before
    //collapse_redundant_matches(match_collection);
    int xcorr_max_rank = get_int_parameter("psms-per-spectrum-reported");
    truncate_match_collection( match_collection, xcorr_max_rank, final_score);
  }

  return num_matches_added;
}

/**
 * \brief Put all the matches from the source match collection in the
 * destination. Only copies the pointers of the matches so use with
 * caution. 
 * \returns The number of matches added.
 */
int merge_match_collections(MATCH_COLLECTION_T* source,
                            MATCH_COLLECTION_T* destination){
  if( source == NULL || destination == NULL ){
    carp(CARP_ERROR, "Cannot merge null match collections.");
  }
  carp(CARP_DETAILED_DEBUG, "Merging match collections.");

  // what is the index of the next insert position in destination
  int dest_idx = destination->match_total;

  // if these are the first being added to the destination, set the
  // scored_type
  if( dest_idx == 0 ){
    int type_idx = 0;
    for(type_idx = 0; type_idx < _SCORE_TYPE_NUM; type_idx++){
      destination->scored_type[type_idx] = source->scored_type[type_idx];
    }
  }else{ // check that same types are scored
    int type_idx = 0;
    for(type_idx = 0; type_idx < _SCORE_TYPE_NUM; type_idx++){
      if( destination->scored_type[type_idx] != source->scored_type[type_idx]){
        char type_str[SMALL_BUFFER];
        char* dest_str = (destination->scored_type[type_idx]) ? "" : " not";
        char* src_str = (source->scored_type[type_idx]) ? "" : " not";
        scorer_type_to_string((SCORER_TYPE_T)type_idx, type_str);
        carp(CARP_FATAL, "Cannot merge match collections scored for "
             "different types.  Trying to add matches%s scored for %s "
             "to matches%s scored for %s", 
             src_str, type_str, dest_str, type_str);
      }
    }
  }
  

  // make sure destination has room for more matches
  int src_num_matches = source->match_total;
  if( dest_idx + src_num_matches > _MAX_NUMBER_PEPTIDES ){
    carp(CARP_FATAL, "Cannot merge match collections, insufficient capacity "
         "in destnation collection.");
  }
  carp(CARP_DETAILED_DEBUG, "Merging %d matches into a collection of %d",
       src_num_matches, dest_idx );

  int src_idx = 0;
  // for each match in source
  for(src_idx = 0; src_idx < src_num_matches; src_idx++){
    MATCH_T* cur_match = source->match[src_idx];

    // copy pointer and add to destination
    increment_match_pointer_count(cur_match);
    destination->match[dest_idx] = cur_match;

    dest_idx++;
  }

  // update destination count
  destination->match_total += src_num_matches;
  destination->experiment_size += source->experiment_size;
  destination->last_sorted = -1;  // unset any last-sorted flag

  return src_num_matches;
}


/**
 * \brief Store the xcorr for each psm that was added in this
 * iteration.  Assumes that the matches with scores needing storing
 * are between indexes start_index and match_collection->match_total.
 * The xcorrs will used for the weibull parameter estimations for
 * p-values.  If keep_matches == FALSE, the matches between indexes
 * start_index and match_collection->match_total will be deleted and
 * match_total will be updated.
 * 
 */
void store_new_xcorrs(
  MATCH_COLLECTION_T* match_collection, ///< source and destination of scores
  int start_index, ///< get first score from match at this index
  BOOLEAN_T keep_matches ///< FALSE=delete the matches after storing score
){

  if( match_collection == NULL ){
    carp(CARP_FATAL, "Cannot store scores of NULL match collection.");
  }

  int score_idx = match_collection->num_xcorrs;
  int psm_idx = start_index;

  carp(CARP_DETAILED_DEBUG, 
       "Adding to xcors[%i] scores from psm index %i to %i", 
       score_idx, psm_idx, match_collection->match_total);

  if( score_idx+(match_collection->match_total-psm_idx) 
      > _MAX_NUMBER_PEPTIDES ){
    carp(CARP_FATAL, "Too many xcorrs to store.");
  }

  for(psm_idx=start_index; psm_idx < match_collection->match_total; psm_idx++){
    FLOAT_T score = get_match_score( match_collection->match[psm_idx], XCORR);
    match_collection->xcorrs[score_idx] = score;
    score_idx++;

    if( keep_matches == FALSE ){
      free_match(match_collection->match[psm_idx]);
      match_collection->match[psm_idx] = NULL;
      match_collection->experiment_size -= 1;  // these should be decoys and 
                                               // we are not counting them
                                               
    }
  }

  match_collection->num_xcorrs = score_idx;
  if( keep_matches == FALSE ){
    match_collection->match_total = start_index; // where we started deleting
  }
  carp(CARP_DETAILED_DEBUG, "There are now %i xcorrs.", score_idx);
}


/**
 * \brief After psms have been added to a match collection but before
 * the collection has been truncated, go through the list of matches
 * and combine those that are for the same peptide sequence.
 *
 * Requires that the match_collection was sorted by Sp so that
 * matches with identical peptides will be listed together.
 */
void collapse_redundant_matches(MATCH_COLLECTION_T* match_collection){
  if( match_collection == NULL ){
    carp(CARP_FATAL, "Cannot collapse matches from null collection.");
  }

  // must not be empty
  int match_total = match_collection->match_total;
  if( match_total == 0 ){
    return;
  }  

  carp(CARP_DETAILED_DEBUG, "Collapsing %i redundant matches.", match_total);

  // must be sorted by Sp or xcorr
  assert( (match_collection->last_sorted == SP) || 
          (match_collection->last_sorted == XCORR) );

  MATCH_T** matches = match_collection->match;
  int match_idx = 0;
  FLOAT_T cur_score = get_match_score(matches[match_idx], SP);

  // for entire list of matches
  while(match_idx < match_total-1){
    FLOAT_T next_score = get_match_score(matches[match_idx+1], SP);

    // find the index of the last match with the same score
    int cur_score_last_index = match_idx;
    
    while(next_score == cur_score && cur_score_last_index < match_total-2){
      cur_score_last_index++;
      next_score = get_match_score(matches[cur_score_last_index+1], SP);
    }
    // if the last two were equal, the last index was not incremented
    if( next_score == cur_score ){ cur_score_last_index++; }

    if( cur_score_last_index > match_idx ){
      consolidate_matches(matches, match_idx, cur_score_last_index);
    }

    match_idx = cur_score_last_index+1;
    cur_score = next_score;
  }// next match

  // shift contents of the match array to fill in deleted matches
  int opening_idx = 0;
  while( matches[opening_idx] != NULL && opening_idx < match_total){
    opening_idx++;
  }

  for(match_idx=opening_idx; match_idx<match_total; match_idx++){
    if( matches[match_idx] != NULL ){ // then move to opening
      matches[opening_idx] = matches[match_idx];
      opening_idx++;
    }
  }

  carp(CARP_DETAILED_DEBUG, "Removing duplicates changed count from %i to %i",
       match_collection->match_total, opening_idx);
  // reset total number of matches in the collection
  match_collection->match_total = opening_idx;
  // remove duplicate peptides from the overall count
  int diff = match_total - opening_idx;
  carp(CARP_DETAILED_DEBUG, "Removing %i from total count %i",
       diff, match_collection->experiment_size);

  match_collection->experiment_size -= diff;
}

/**
 * \brief For a list of matches with the same scores, combine those
 * that are the same peptide and delete redundant matches.
 *
 * Since there may be different peptide sequences with the same score,
 * compare each match to the remaining matches.
 */
void consolidate_matches(MATCH_T** matches, int start_idx, int end_idx){

  carp(CARP_DETAILED_DEBUG, "Consolidating index %i to %i.", start_idx, end_idx);
  int cur_match_idx = 0;
  for(cur_match_idx=start_idx; cur_match_idx < end_idx; cur_match_idx++){
    carp(CARP_DETAILED_DEBUG, "Try consolidating with match[%i].", 
         cur_match_idx);

    if(matches[cur_match_idx] == NULL){
      carp(CARP_DETAILED_DEBUG, "Can't consolodate with %i, it's null.", 
           cur_match_idx);
      continue;
    }    

    char* cur_seq = get_match_mod_sequence_str(matches[cur_match_idx]);
    carp(CARP_DETAILED_DEBUG, "cur seq is %s.", cur_seq);
    int next_match_idx = cur_match_idx+1;
    for(next_match_idx=cur_match_idx+1; next_match_idx<end_idx+1; 
        next_match_idx++){
      carp(CARP_DETAILED_DEBUG, "Can match[%i] be added to cur.", 
           next_match_idx);

      if(matches[next_match_idx] == NULL){
        continue;
      }    

      char* next_seq = get_match_mod_sequence_str(matches[next_match_idx]);
      carp(CARP_DETAILED_DEBUG, "next seq is %s.", next_seq);

      if( strcmp(cur_seq, next_seq) == 0){
        carp(CARP_DETAILED_DEBUG, 
             "Seqs %s and %s match.  Consolidate match[%i] into match[%i].", 
             cur_seq, next_seq, next_match_idx, cur_match_idx);

        // add peptide src of next to cur
        merge_peptides_copy_src( get_match_peptide(matches[cur_match_idx]),
                        get_match_peptide(matches[next_match_idx]));
        // this frees the second peptide, so set what pointed to it to NULL
        //set_match_peptide(matches[next_match_idx], NULL);

        // delete match
        free_match(matches[next_match_idx]);
        matches[next_match_idx] = NULL;
      }

      free(next_seq);
    }// next match to delete

    free(cur_seq);
  }// next match to consolidate to


}


/**
 * sort the match collection by score_type(SP, XCORR, ... )
 *\returns TRUE, if successfully sorts the match_collection
 */
BOOLEAN_T sort_match_collection(
  MATCH_COLLECTION_T* match_collection, ///< the match collection to sort -out
  SCORER_TYPE_T score_type ///< the score type (SP, XCORR) to sort by -in
  )
{
  // check if we are allowed to alter match_collection
  if(match_collection->iterator_lock){
    carp(CARP_ERROR, 
         "Cannot alter match_collection when a match iterator is already"
         " instantiated");
    return FALSE;
  }

  switch(score_type){
  case DOTP:
    // implement later
    return FALSE;

  case XCORR:
//case LOGP_EVD_XCORR:
  case LOGP_BONF_EVD_XCORR:
  case LOGP_WEIBULL_XCORR: 
    // LOGP_BONF_EVD_XCORR and XCORR have same order, 
    // sort the match to decreasing XCORR order for the return
    qsort_match(match_collection->match, match_collection->match_total, 
                (void *)compare_match_xcorr);
    match_collection->last_sorted = XCORR;
    return TRUE;

  case LOGP_BONF_WEIBULL_XCORR: 
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_p_value);
    match_collection->last_sorted = LOGP_BONF_WEIBULL_XCORR;
    return TRUE;

  case SP: 
  case LOGP_EXP_SP: 
    //case LOGP_BONF_EXP_SP: 
  case LOGP_WEIBULL_SP: 
  case DECOY_XCORR_QVALUE:
  case DECOY_PVALUE_QVALUE:
  case LOGP_BONF_WEIBULL_SP: 
  case LOGP_QVALUE_WEIBULL_XCORR: // should this be here?
    // LOGP_EXP_SP and SP have same order, 
    // thus sort the match to decreasing SP order for the return
    carp(CARP_DETAILED_DEBUG, "Sorting match_collection of %i matches", 
         match_collection->match_total);
    qsort_match(match_collection->match, 
                match_collection->match_total, (void *)compare_match_sp);
    match_collection->last_sorted = SP;
    return TRUE;

  case Q_VALUE:
  case PERCOLATOR_SCORE:
    qsort_match(match_collection->match, match_collection->match_total, 
        (void *)compare_match_percolator_score);
    match_collection->last_sorted = PERCOLATOR_SCORE;
    return TRUE;

  case QRANKER_Q_VALUE:
  case QRANKER_SCORE:
    qsort_match(match_collection->match, match_collection->match_total, 
        (void *)compare_match_qranker_score);
    match_collection->last_sorted = QRANKER_SCORE;
    return TRUE;
  }
  return FALSE;
}

/**
 * \brief Sort a match_collection by the given score type, grouping
 * matches by spectrum (if multiple spectra present).
 * \returns TRUE if sort is successful, else FALSE;
 */
BOOLEAN_T spectrum_sort_match_collection(
  MATCH_COLLECTION_T* match_collection, ///< match collection to sort -out
  SCORER_TYPE_T score_type ///< the score type to sort by -in
  ){

  BOOLEAN_T success = FALSE;

  // check if we are allowed to alter match_collection
  if(match_collection->iterator_lock){
    carp(CARP_ERROR, 
         "Cannot alter match_collection when a match iterator is already"
         " instantiated");
    return FALSE;
  }

  switch(score_type){
  case DOTP:
    success = FALSE;
    break;

  case XCORR:
    //case LOGP_EVD_XCORR:
  case LOGP_BONF_EVD_XCORR:
  case LOGP_WEIBULL_XCORR: 
  case LOGP_BONF_WEIBULL_XCORR: 
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_xcorr);
    match_collection->last_sorted = XCORR;
    success = TRUE;
    break;

  case SP: 
  case LOGP_EXP_SP: 
    //case LOGP_BONF_EXP_SP: 
  case LOGP_WEIBULL_SP: 
  case LOGP_BONF_WEIBULL_SP: 
  case LOGP_QVALUE_WEIBULL_XCORR: 
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_sp);
    match_collection->last_sorted = SP;
    success = TRUE;
    break;

  case Q_VALUE:
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_q_value);
    match_collection->last_sorted = Q_VALUE;
    success = TRUE;
    break;

  case QRANKER_Q_VALUE:
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_qranker_q_value);
    match_collection->last_sorted = QRANKER_Q_VALUE;
    success = TRUE;
    break;

  case PERCOLATOR_SCORE:
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_percolator_score);
    match_collection->last_sorted = PERCOLATOR_SCORE;
    success = TRUE;
    break;

  case QRANKER_SCORE:
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_qranker_score);
    match_collection->last_sorted = QRANKER_SCORE;
    success = TRUE;
    break;

  case DECOY_XCORR_QVALUE:
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_decoy_xcorr_qvalue);
    match_collection->last_sorted = DECOY_XCORR_QVALUE;
    success = TRUE;
    break;

  case DECOY_PVALUE_QVALUE:
    qsort_match(match_collection->match, match_collection->match_total,
                (void*)compare_match_spectrum_decoy_pvalue_qvalue);
    match_collection->last_sorted = DECOY_PVALUE_QVALUE;
    success = TRUE;
    break;


  }

  return success;
}

/**
 * \brief Reduces the number of matches in the match_collection so
 * that only the <max_rank> highest scoring (by score_type) remain.
 *
 * Matches ranking up to max_rank are retained and those ranking
 * higher are freed.  The value of match_collection->total_matches is
 * adjusted to reflect the remaining number of matches.  The max rank
 * and total_matches may not be the same value if there are multiple
 * matches with the same rank.  Sorts match collection by score_type,
 * if necessary.
 */
void truncate_match_collection(
  MATCH_COLLECTION_T* match_collection, ///< match collection to truncate -out
  int max_rank,     ///< max rank of matches to keep from SP -in
  SCORER_TYPE_T score_type ///< the score type (SP, XCORR) -in
  )
{
  carp(CARP_DETAILED_DEBUG, "Truncating match collection.");
  if (match_collection == NULL || match_collection->match_total == 0){
    carp(CARP_DETAILED_DEBUG, "No matches in collection, so not truncating");
    return;
  }

  // sort match collection by score type
  // check if the match collection is in the correct sorted order
  if(match_collection->last_sorted != score_type){
    // sort match collection by score type
    if(!sort_match_collection(match_collection, score_type)){
      carp(CARP_FATAL, "Failed to sort match collection");
    }
  }

  // Free high ranking matches
  int highest_index = match_collection->match_total -1;
  int cur_last_rank = get_match_rank(match_collection->match[highest_index],
                                     score_type);
  while( cur_last_rank > max_rank ){
    free_match(match_collection->match[highest_index]);
    highest_index--;
    cur_last_rank = get_match_rank(match_collection->match[highest_index],
                                   score_type);
  }
  match_collection->match_total = highest_index+1;
}

/**
 * Assigns a rank for the given score type to each match.  First sorts
 * by the score type (if not already sorted).  Overwrites any existing
 * rank values, so it can be performed on a collection with matches
 * newly added to previously ranked matches.  Rank 1 is highest
 * score.  Matches with the same score will be given the same rank.
 *
 * \returns TRUE, if populates the match rank in the match collection
 */
BOOLEAN_T populate_match_rank_match_collection(
 MATCH_COLLECTION_T* match_collection, ///< match collection to rank -out
 SCORER_TYPE_T score_type ///< score type (SP, XCORR) by which to rank -in
 )
{
  carp(CARP_DETAILED_DEBUG, "Ranking matches by %i.", score_type);
  carp(CARP_DETAILED_DEBUG, "Collection currently ranked by %d", match_collection->last_sorted);
  // check if the match collection is in the correct sorted order
  if(match_collection->last_sorted != score_type){
    // sort match collection by score type
    carp(CARP_DETAILED_DEBUG, "Sorting by score_type %i", score_type);
    if(!sort_match_collection(match_collection, score_type)){
      carp(CARP_ERROR, "Failed to sort match collection");
      return FALSE;
    }
  }

  // set match rank for all match objects that have been scored for
  // this type
  int match_index;
  int cur_rank = 0;
  FLOAT_T cur_score = NOT_SCORED;
  for(match_index=0; match_index<match_collection->match_total; ++match_index){
    MATCH_T* cur_match = match_collection->match[match_index];
    FLOAT_T this_score = get_match_score(cur_match, score_type);
    
    if( NOT_SCORED == get_match_score(cur_match, score_type) ){
      char* seq = get_match_mod_sequence_str(cur_match);
      carp(CARP_WARNING, 
           "PSM spectrum %i charge %i sequence %s was NOT scored for type %i",
           get_spectrum_first_scan(get_match_spectrum(cur_match)),
           get_match_charge(cur_match), seq,
           (int)score_type);
      free(seq);
    }

    // does this match have a higher score?
    if( this_score != cur_score ){
      cur_score = this_score;
      cur_rank++;
    }

    //    set_match_rank( cur_match, score_type, match_index+1);
    set_match_rank( cur_match, score_type, cur_rank);

    carp(CARP_DETAILED_DEBUG, "Match rank %i, score %f", cur_rank, cur_score);
  }
  
  //carp(CARP_DETAILED_DEBUG, "Max rank %i", match_index);
  return TRUE;
}

/**
 * Create a new match_collection by randomly sampling matches 
 * from match_collection upto count number of matches
 * Must not free the matches
 * \returns a new match_collection of randomly sampled matches 
 */
MATCH_COLLECTION_T* random_sample_match_collection(
  MATCH_COLLECTION_T* match_collection, ///< the match collection to sample -out
  int count_max ///< the number of matches to randomly select -in
  )
{
  int count_idx = 0;
  int match_idx = 0;
  int score_type_idx = 0;
  MATCH_COLLECTION_T* sample_collection = allocate_match_collection();
  srand(time(NULL));

  // make sure we don't sample more than the matches in the match collection
  if (count_max >= match_collection->match_total){
    free_match_collection(sample_collection);
    return match_collection;
  }

  // ranomly select matches upto count_max
  for(; count_idx < count_max; ++count_idx){
    match_idx = ((double)rand()/((double)RAND_MAX + (double)1)) 
      * match_collection->match_total;
    
    // match_idx = rand() % match_collection->match_total;
    sample_collection->match[count_idx] = match_collection->match[match_idx];
    // increment pointer count of the match object 
    increment_match_pointer_count(sample_collection->match[count_idx]);
  }
  
  // set total number of matches sampled
  sample_collection->match_total = count_idx;

  sample_collection->experiment_size = match_collection->experiment_size;

  // set scored types in the sampled matches
  for(; score_type_idx < _SCORE_TYPE_NUM;  ++score_type_idx){
    sample_collection->scored_type[score_type_idx] 
      = match_collection->scored_type[score_type_idx];
  }
  
  return sample_collection;
}

/**
 * This function is a transformation of the partial derivatives of
 * the log likelihood of the data given an extreme value distribution
 * with location parameter mu and scale parameter 1/L. The transformation 
 * has eliminated the explicit dependence on the location parameter, mu, 
 * leaving only the scale parameter, 1/L.
 *
 * The zero crossing of this function will correspond to the maximum of the 
 * log likelihood for the data.
 *
 * See equations 10 and 11 of "Maximum Likelihood fitting of extreme value 
 * distributions".
 *
 * The parameter values contains a list of the data values.
 * The parameter L is the reciprocal of the scale parameters.
 *
 *\returns the final exponential values of the score and sets the value of the function and its derivative.
 */
void constraint_function(
  MATCH_COLLECTION_T* match_collection, ///< the match collection to estimate evd parameters -in
  SCORER_TYPE_T score_type, ///< score_type to estimate EVD distribution -in
  FLOAT_T l_value,  ///< L value -in
  FLOAT_T* function,  ///< the output function value -out
  FLOAT_T* derivative,  ///< the output derivative value -out
  FLOAT_T* exponential_sum ///< the final exponential array sum -out
  )
{
  int idx = 0;
  FLOAT_T* exponential = (FLOAT_T*)mycalloc(match_collection->match_total, sizeof(FLOAT_T));
  FLOAT_T numerator = 0;
  FLOAT_T second_numerator = 0;
  FLOAT_T score = 0;
  FLOAT_T denominator = 0;
  FLOAT_T score_sum = 0;
  MATCH_T** matches = match_collection->match;

  // iterate over the matches to calculate numerator, exponential value, denominator
  for(; idx < match_collection->match_total; ++idx){
    score = get_match_score(matches[idx], score_type);
    exponential[idx] = exp(-l_value * score);
    numerator += (exponential[idx] * score);
    denominator += exponential[idx];
    score_sum += score;
    second_numerator += (score * score * exponential[idx]);
  }

  // assign function value
  *function = (1.0 / l_value) - (score_sum / match_collection->match_total) 
    + (numerator / denominator);

  // assign derivative value
  *derivative =  ((numerator * numerator) / (denominator * denominator)) 
    - ((second_numerator / denominator)) - (1.0 / (l_value * l_value));

  // assign the total sum of the exponential values
  *exponential_sum = denominator;

  // free exponential array
  free(exponential);
}


/**
 * For the #top_count ranked peptides, calculate the Weibull parameters
 *\returns TRUE, if successfully calculates the Weibull parameters
 */
#define MIN_WEIBULL_MATCHES 40
#define MIN_XCORR_SHIFT -5.0
#define MAX_XCORR_SHIFT  5.0
//#define CORR_THRESHOLD 0.995   // Must achieve this correlation, else punt.
#define CORR_THRESHOLD 0.0       // For now, turn off the threshold.
#define XCORR_SHIFT 0.05
#define MIN_SP_SHIFT -100.0
#define MAX_SP_SHIFT 300.0
#define SP_SHIFT 5.0

/**
 * \brief Check that a match collection has a sufficient number of
 * matches for estimating Weibull parameters.
 * \returns TRUE if their are enough xcorrs for estimating Weibull
 * parameters or FALSE if not.
 */
BOOLEAN_T has_enough_weibull_points(
  MATCH_COLLECTION_T* match_collection
){
  return (match_collection->num_xcorrs >= MIN_WEIBULL_MATCHES );
}

/**
 * \brief Use the xcorrs saved in the match_collection to estimate the
 * weibull parameters to be used for computing p-values. 
 *
 * Requires that main score be XCORR, but with relatively few changes
 * other scores could be accomodated.
 * Implementation of Weibull distribution parameter estimation from 
 * http:// www.chinarel.com/onlincebook/LifeDataWeb/rank_regression_on_y.htm
 */
BOOLEAN_T estimate_weibull_parameters_from_xcorrs(
  MATCH_COLLECTION_T* match_collection, 
  SPECTRUM_T* spectrum,
  int charge
  ){

  if( match_collection == NULL || spectrum == NULL ){
    carp(CARP_ERROR, "Cannot estimate parameters from null inputs.");
    return FALSE;
  }

  // check that we have the minimum number of matches
  FLOAT_T* scores = match_collection->xcorrs;
  int num_scores = match_collection->num_xcorrs;
  if( num_scores < MIN_WEIBULL_MATCHES ){
    carp(CARP_DETAILED_DEBUG, "Too few psms (%i) to estimate "
         "p-value parameters for spectrum %i, charge %i",
         num_scores, get_spectrum_first_scan(spectrum), charge);
    // set eta, beta, and shift to something???
    return FALSE;
  }

  // reverse sort the scores
  qsort(scores, num_scores, sizeof(FLOAT_T), compare_floats_descending);

  // use only a fraction of the samples, the high-scoring tail
  // this parameter is hidden from the user
  double fraction_to_fit = get_double_parameter("fraction-top-scores-to-fit");
  assert( fraction_to_fit >= 0 && fraction_to_fit <= 1 );
  int num_tail_samples = (int)(num_scores * fraction_to_fit);
  carp(CARP_DEBUG, "Estimating Weibull params with %d psms (%.2f of %i)", 
       num_tail_samples, fraction_to_fit, num_scores);

  // do the estimation
  fit_three_parameter_weibull(scores, num_tail_samples, num_scores,
      MIN_XCORR_SHIFT, MAX_XCORR_SHIFT, XCORR_SHIFT, CORR_THRESHOLD,
      &(match_collection->eta), &(match_collection->beta),
      &(match_collection->shift), &(match_collection->correlation));
  carp(CARP_DEBUG, 
      "Corr: %.6f  Eta: %.6f  Beta: %.6f  Shift: %.6f", 
       match_collection->correlation, match_collection->eta, 
       match_collection->beta, match_collection->shift);
  
  return TRUE;
}

// TODO (BF 16-mar-09): use this instead of score_peptides
/**
 * \brief Add all peptides from iterator to match collection.
 * Additional matches will not be scored for any type.
 * \returns TRUE if successful.
 */
BOOLEAN_T add_unscored_peptides(
  MATCH_COLLECTION_T* match_collection, 
  SPECTRUM_T* spectrum, 
  int charge, 
  MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator,
  BOOLEAN_T is_decoy
){

  if( match_collection == NULL || spectrum == NULL 
      || peptide_iterator == NULL ){
    carp(CARP_FATAL, "Cannot score peptides with NULL inputs.");
  }
  carp(CARP_DETAILED_DEBUG, "Adding decoy peptides to match collection? %i", 
       is_decoy);

  int starting_number_of_psms = match_collection->match_total;

  while( modified_peptides_iterator_has_next(peptide_iterator)){
    // get peptide
    PEPTIDE_T* peptide = modified_peptides_iterator_next(peptide_iterator);

    // create a match
    MATCH_T* match = new_match();

    // set match fields
    set_match_peptide(match, peptide);
    set_match_spectrum(match, spectrum);
    set_match_charge(match, charge);
    set_match_null_peptide(match, is_decoy);

    // add to match collection
    if(match_collection->match_total >= _MAX_NUMBER_PEPTIDES){
      carp(CARP_ERROR, "peptide count of %i exceeds max match limit: %d", 
          match_collection->match_total, _MAX_NUMBER_PEPTIDES);

      return FALSE;
    }

    match_collection->match[match_collection->match_total] = match;
    match_collection->match_total++;

  }// next peptide

  int matches_added = match_collection->match_total - starting_number_of_psms;
  match_collection->experiment_size += matches_added;

  return TRUE;
}


/**
 * \brief Compare all peptides in iterator to spectrum using score
 * type and store results in match collection.  
 * \return TRUE if successful.
 */
BOOLEAN_T score_peptides(
  SCORER_TYPE_T score_type, ///< use this score to compare spec/peptide
  MATCH_COLLECTION_T* match_collection, ///< put results here
  SPECTRUM_T* spectrum,     ///< spectrum to compare
  int charge,               ///< charge of spectrum
  MODIFIED_PEPTIDES_ITERATOR_T* peptide_iterator, ///< source of peptides
  BOOLEAN_T is_decoy        ///< do we shuffle the peptides
  //BF: this was added so that match_collection can be mixed target/decoy
){

  if( match_collection == NULL || spectrum == NULL 
      || peptide_iterator == NULL ){
    carp(CARP_FATAL, "Cannot score peptides with NULL inputs.");
  }

  // create ion constraint
  ION_CONSTRAINT_T* ion_constraint = 
    new_ion_constraint_smart(score_type, charge);

  // create scorer
  SCORER_T* scorer = new_scorer(score_type);

  // variables to re-use in the loop
  char* sequence = NULL;
  MODIFIED_AA_T* modified_sequence = NULL;
  MATCH_T* match = NULL;
  FLOAT_T score = 0;
  PEPTIDE_T* peptide = NULL;

  carp(CARP_DETAILED_DEBUG, "New match_collection is null? %i", is_decoy);

  // create a generic ion_series that will be reused for each peptide sequence
  ION_SERIES_T* ion_series = new_ion_series_generic(ion_constraint, charge);  

  int starting_number_of_psms = match_collection->match_total;
  carp(CARP_DEBUG, "Scoring all peptides in iterator.");
  while( modified_peptides_iterator_has_next(peptide_iterator)){
    // get peptide, sequence, and ions
    peptide = modified_peptides_iterator_next(peptide_iterator);
    //SJM: Calling this multiple times for each peptide can get expensive.
    //I defined this macro in carp.h that tests the verbosity level
    //before calling the get_ function.  We could use this to compile out
    //all debugging information in order to make a more optimized crux.
    IF_CARP_DETAILED_DEBUG(
      char* seq = get_peptide_modified_sequence(peptide);
      carp(CARP_DETAILED_DEBUG, "peptide %s has %i modified aas", seq, count_peptide_modified_aas(peptide)); 
      free(seq);
    )
    // create a match
    match = new_match();

    // set match fields
    set_match_peptide(match, peptide);
    set_match_spectrum(match, spectrum);
    set_match_charge(match, charge);
    set_match_null_peptide(match, is_decoy);

    // update ion series for peptide sequene
    sequence = get_match_sequence(match);
    modified_sequence = get_match_mod_sequence(match);
    update_ion_series(ion_series, sequence, modified_sequence);
    predict_ions(ion_series);


    // calculate the score
    score = score_spectrum_v_ion_series(scorer, spectrum, ion_series);
    // debugging
    IF_CARP_DETAILED_DEBUG(
      char* mod_seq = modified_aa_string_to_string(modified_sequence, strlen(sequence));
      carp(CARP_DETAILED_DEBUG, "Score %f for %s (null:%i)", score, mod_seq, is_decoy);
      free(mod_seq);
    )

    // set match fields
    set_match_score(match, score_type, score);
    set_match_b_y_ion_info(match, scorer);

    // add to match collection
    if(match_collection->match_total >= _MAX_NUMBER_PEPTIDES){
      carp(CARP_ERROR, "peptide count of %i exceeds max match limit: %d", 
          match_collection->match_total, _MAX_NUMBER_PEPTIDES);
      // free heap
      free(sequence);
      free_ion_series(ion_series);
      free_scorer(scorer);
      free_ion_constraint(ion_constraint);
      free(modified_sequence);

      return FALSE;
    }

    match_collection->match[match_collection->match_total] = match;
    match_collection->match_total++;
    match_collection->sp_scores_sum += score;

    free(modified_sequence);
    free(sequence);

  }// next peptide

  int matches_added = match_collection->match_total - starting_number_of_psms;

  // calculate current mean
  match_collection->sp_scores_mean = match_collection->sp_scores_sum
                                      / match_collection->match_total;
  //match_collection->experiment_size = match_collection->match_total;
  match_collection->experiment_size += matches_added;

  // mark it as scored
  match_collection->scored_type[score_type] = TRUE;

  // Let caller do sorting 

  // clean up
  free_ion_series(ion_series);
  free_scorer(scorer);
  free_ion_constraint(ion_constraint);

  return TRUE;
}

/**
 * Main scoring methods
 * 
 * Unlike preliminary scoring methods only updates existing match objects 
 * with new scores. In all cases should get peptide sequence only through 
 * get_match_sequence method.
 */

/**
 * \brief Use the score type to compare the spectrum and peptide in
 * the matches in match collection.  Scores only the first n where n
 * is defined by the parameter.c param, max-rank-preliminary.
 *
 * SPEED-VS-GENREALITY TRADEOFF: Requires that all of the matches
 * already hold the given spectrum.  This is mostly an issue of the
 * max charge for the ion constraint.  If a new ion constraint is
 * created each time, or if the charge on the constraint is updated
 * each time, do not have to pass the spectrum or charge.
 *
 * \returns TRUE, if matches are successfully scored.
 */
BOOLEAN_T score_matches_one_spectrum(
  SCORER_TYPE_T score_type, 
  MATCH_T** matches,
  int num_matches,
  SPECTRUM_T* spectrum,
  int charge
  ){

  char type_str[64];
  scorer_type_to_string(score_type, type_str);
  carp(CARP_DETAILED_DEBUG, "Scoring matches for %s", type_str);

  if( matches == NULL || spectrum == NULL ){
    carp(CARP_ERROR, "Cannot score matches in a NULL match collection.");
    return FALSE;
  }
  
  // create ion constraint
  ION_CONSTRAINT_T* ion_constraint = new_ion_constraint_smart(score_type, 
                                                              charge);
  // create scorer
  SCORER_T* scorer = new_scorer(score_type);

  // create a generic ion_series that will be reused for each peptide sequence
  ION_SERIES_T* ion_series = new_ion_series_generic(ion_constraint, charge);  
  
  // score all matches
  int match_idx;
  MATCH_T* match = NULL;
  char* sequence = NULL;
  MODIFIED_AA_T* modified_sequence = NULL;

  for(match_idx = 0; match_idx < num_matches; match_idx++){

    match = matches[match_idx];
    assert( match != NULL );

    // skip it if it's already been scored
    if( NOT_SCORED != get_match_score(match, score_type)){
      continue;
    }

    // make sure it's the same spec and charge
    assert( spectrum == get_match_spectrum(match));
    assert( charge == get_match_charge(match));
    sequence = get_match_sequence(match);
    modified_sequence = get_match_mod_sequence(match);

    // create ion series for this peptide
    update_ion_series(ion_series, sequence, modified_sequence);
    predict_ions(ion_series);

    // get the score
    FLOAT_T score = score_spectrum_v_ion_series(scorer, spectrum, ion_series);

    // set score in match
    set_match_score(match, score_type, score);
    
    IF_CARP_DETAILED_DEBUG(
      char* mod_seq = modified_aa_string_to_string(modified_sequence,
						   strlen(sequence));
      carp(CARP_DETAILED_DEBUG, "Second score %f for %s (null:%i)",
	   score, mod_seq,get_match_null_peptide(match));
      free(mod_seq);
    )
    free(sequence);
    free(modified_sequence);
  }// next match

  // clean up
  free_ion_constraint(ion_constraint);
  free_ion_series(ion_series);
  free_scorer(scorer);
  return TRUE;
}

/**
 * \brief  Uses the Weibull parameters estimated by
 * estimate_weibull_parameters() to compute a p-value for each psm in
 * the collection.
 *
 * Computes the p-value for score-type set in parameter.c (which should
 * have been used for estimating the parameters).  Stores scores at
 * match->match_scores[LOGP_BONF_WEIBULL_XCORR].  This function
 * previous performed in score_collection_logp_bonf_weibull_[xcorr,sp]. 
 * 
 * \returns TRUE if p-values successfully computed for all matches,
 * else FALSE.
 */
// FIXME (BF 8-Dec-2008): create new score-type P_VALUE to replace LOG...XCORR
BOOLEAN_T compute_p_values(
  MATCH_COLLECTION_T* match_collection,
  FILE* output_pvalue_file ///< If non-NULL, file for storing p-values -in
  ){

  if(match_collection == NULL){
    carp(CARP_ERROR, "Cannot compute p-values for NULL match collection.");
    return FALSE;
  }

  int scan_number
    = get_spectrum_first_scan(get_match_spectrum(match_collection->match[0]));
  carp(CARP_DEBUG, "Computing p-values for %s spec %d charge %d "
       "with eta %f beta %f shift %f",
       (match_collection->null_peptide_collection) ? "decoy" : "target",
       scan_number,
       match_collection->charge,
       match_collection->eta, match_collection->beta, match_collection->shift);

  SCORER_TYPE_T main_score = get_scorer_type_parameter("score-type");

  // check that the matches have been scored
  if(!match_collection->scored_type[main_score]){
    char type_str[64];
    scorer_type_to_string(main_score, type_str);
    carp(CARP_FATAL, 
         "Match collection was not scored by %s prior to computing p-values.",
         type_str);
  }

  // Print separator in the decoy p-value file.
  if (output_pvalue_file) {
    fprintf(output_pvalue_file, "# scan: %d charge: %d candidates: %d\n", 
	    scan_number, match_collection->charge,
	    match_collection->experiment_size);
    fprintf(output_pvalue_file, 
	    "# eta: %g beta: %g shift: %g correlation: %g\n",
	    match_collection->eta, 
	    match_collection->beta,
	    match_collection->shift,
	    match_collection->correlation);
  }

  // iterate over all matches 
  int match_idx =0;
  for(match_idx=0; match_idx < match_collection->match_total; match_idx++){
    MATCH_T* cur_match = match_collection->match[match_idx];

    // Get the Weibull p-value.
    double pvalue = compute_weibull_pvalue(get_match_score(cur_match, 
							   main_score),
					   match_collection->eta, 
					   match_collection->beta,
					   match_collection->shift);

    // Print the pvalue, if requested
    if (output_pvalue_file) {
      fprintf(output_pvalue_file, "%g\n", pvalue);
    }

    // Apply the Bonferroni correction.
    pvalue = bonferroni_correction(pvalue, match_collection->experiment_size);

    // set pvalue in match
    set_match_score(cur_match, LOGP_BONF_WEIBULL_XCORR, -log(pvalue));
    //#endif

  }// next match

  carp(CARP_DETAILED_DEBUG, "Computed p-values for %d PSMs.", match_idx);
  populate_match_rank_match_collection(match_collection, XCORR);

  // mark p-values as having been scored
  match_collection->scored_type[LOGP_BONF_WEIBULL_XCORR] = TRUE;
  return TRUE;
}

/**
 * \brief Use the matches collected from all spectra to compute FDR
 * and q_values from the ranked list of target and decoy scores.
 * Requires that matches have been scored for the given score type.
 * Assumes the match_collection has an appropriate number of
 * target/decoy matches per spectrum (e.g. one target and one decoy
 * per spec).  If p-value is NaN for a psm, q-value will also be NaN.
 * \returns TRUE if q-values successfully computed, else FALSE.
 */
BOOLEAN_T compute_decoy_q_values(
 MATCH_COLLECTION_T* match_collection,///< m_c with matches from many spec
 SCORER_TYPE_T score_type) ///< type to sort by (xcorr or p-value)
{

  if( match_collection == NULL ){
    carp(CARP_ERROR, "Cannot compute q-values for null match collection.");
    return FALSE;
  }

  carp(CARP_DEBUG, "Computing decoy q-values for score type %i.",
       score_type);

  // sort by score
  sort_match_collection(match_collection, score_type);

  // which q_val type are we using
  SCORER_TYPE_T qval_type;
  if( score_type == XCORR ){
    qval_type = DECOY_XCORR_QVALUE;
  }else if( score_type == LOGP_BONF_WEIBULL_XCORR ){
    qval_type = DECOY_PVALUE_QVALUE;
  }else{
    char buf[SMALL_BUFFER];
    scorer_type_to_string(score_type, buf);
    carp(CARP_ERROR, "Don't know where to store q-values for score type %s.",
         buf);
    return FALSE;
  }

  // compute FDR from a running total of number targets/decoys
  // FDR = #decoys / #targets
  FLOAT_T num_targets = 0;
  FLOAT_T num_decoys = 0;
  int match_idx = 0;
  for(match_idx = 0; match_idx < match_collection->match_total; match_idx++){
    MATCH_T* cur_match = match_collection->match[match_idx];

    // skip if pvalue score is NaN
    if( score_type == LOGP_BONF_WEIBULL_XCORR && 
        get_match_score(cur_match, LOGP_BONF_WEIBULL_XCORR) == P_VALUE_NA){
      set_match_score(cur_match, qval_type, P_VALUE_NA);
      continue;
    }

    if( get_match_null_peptide(cur_match) == TRUE ){
      num_decoys += 1;
    }else{
      num_targets += 1;
    }
    FLOAT_T score = num_decoys/num_targets;
    if( num_targets == 0 ){ score = 1.0; }

    set_match_score(cur_match, qval_type, score);
    carp(CARP_DETAILED_DEBUG, 
         "match %i xcorr or pval %f num targets %i, num decoys %i, score %f",
         match_idx, get_match_score(cur_match, score_type), 
         (int)num_targets, (int)num_decoys, score);
  }

  // compute q-value: go through list in reverse and use min FDR seen
  FLOAT_T min_fdr = 1.0;
  for(match_idx = match_collection->match_total-1; match_idx >= 0; match_idx--){
    MATCH_T* cur_match = match_collection->match[match_idx];
    FLOAT_T cur_fdr = get_match_score(cur_match, qval_type);
    if( cur_fdr == P_VALUE_NA ){ continue; }

    if( cur_fdr < min_fdr ){
      min_fdr = cur_fdr;
    }

    set_match_score(cur_match, qval_type, min_fdr);
    carp(CARP_DETAILED_DEBUG, 
         "match %i cur fdr %f min fdr %f is decoy %i",
         match_idx, cur_fdr, min_fdr, get_match_null_peptide(cur_match) );
  }

  match_collection->scored_type[qval_type] = TRUE;
  return TRUE;
}

/**
 * match_collection get, set method
 */

/**
 *\returns TRUE, if the match collection has been scored by score_type
 */
BOOLEAN_T get_match_collection_scored_type(
  MATCH_COLLECTION_T* match_collection, ///< the match collection to iterate -in
  SCORER_TYPE_T score_type ///< the score_type (MATCH_SP, MATCH_XCORR) -in
  )
{
  return match_collection->scored_type[score_type];
}

/**
 * sets the score_type to value
 */
void set_match_collection_scored_type(
  MATCH_COLLECTION_T* match_collection, ///< the match collection to iterate -in
  SCORER_TYPE_T score_type, ///< the score_type (MATCH_SP, MATCH_XCORR) -in
  BOOLEAN_T value
  )
{
  match_collection->scored_type[score_type] = value;
}

/**
 *\returns TRUE, if there is a  match_iterators instantiated by match collection 
 */
BOOLEAN_T get_match_collection_iterator_lock(
  MATCH_COLLECTION_T* match_collection ///< working match collection -in
  )
{
  return match_collection->iterator_lock;
}

/**
 *\returns the total match objects avaliable in current match_collection
 */
int get_match_collection_match_total(
  MATCH_COLLECTION_T* match_collection ///< working match collection -in
  )
{
  return match_collection->match_total;
}

/**
 *\returns the total peptides searched in the experiment in match_collection
 */
int get_match_collection_experimental_size(
  MATCH_COLLECTION_T* match_collection ///< working match collection -in
  )
{
  return match_collection->experiment_size;
}

/**
 *\returns the top peptide count used in the logp_exp_sp in match_collection
 */
int get_match_collection_top_fit_sp(
  MATCH_COLLECTION_T* match_collection ///< working match collection -in
  )
{
  return match_collection->top_fit_sp;
}

/**
 *\returns the charge of the spectrum that the match collection was created
 */
int get_match_collection_charge(
  MATCH_COLLECTION_T* match_collection ///< working match collection -in
  )
{
  return match_collection->charge;
}

/**
 * Must have been scored by Xcorr, returns error if not scored by Xcorr
 *\returns the delta cn value(difference in top and second ranked Xcorr values)
 */
FLOAT_T get_match_collection_delta_cn(
  MATCH_COLLECTION_T* match_collection ///< working match collection -in
  )
{
  // Check if xcorr value has been scored, thus delta cn value is valid
  if(match_collection->scored_type[XCORR]){
    return match_collection->delta_cn;
  }
  else{
    carp(CARP_ERROR, "must score match_collection with XCORR to get delta cn value");
    return 0.0;
  }
}

/**
 * \brief Transfer the Weibull distribution parameters, including the
 * correlation from one match_collection to another.  No check to see
 * that the parameters have been estimated.
 */
void transfer_match_collection_weibull(
  MATCH_COLLECTION_T* from_collection,
  MATCH_COLLECTION_T* to_collection
  ){
  to_collection->eta = from_collection->eta;
  to_collection->beta = from_collection->beta;
  to_collection->shift = from_collection->shift;
  to_collection->correlation = from_collection->correlation;
}

/**
 * \brief Names and opens the correct number of binary psm files.
 *
 * Takes the values of output-dir parameter, ms2 filename (soon to be
 * named output file), overwrite, and num-decoy-files from parameter.c.
 * Exits with error if can't create new requested directory or if
 * can't create any of the psm files.
 * REPLACES: spectrum_collection::get_spectrum_collection_psm_result_filenames
 *
 * \returns An array of filehandles to the newly opened files
 */
FILE** create_psm_files(){

  int decoy_files = get_int_parameter("num-decoy-files");
  int total_files = decoy_files +1;
  // create FILE* array to return
  FILE** file_handle_array = (FILE**)mycalloc(total_files, sizeof(FILE*));
  int file_idx = 0;

  carp(CARP_DEBUG, "Opening %d new psm files", total_files);

  const char* output_directory = get_string_parameter_pointer("output-dir");

  // create the output folder if it doesn't exist
  if(access(output_directory, F_OK)){
    if(mkdir(output_directory, S_IRWXU+S_IRWXG+S_IRWXO) != 0){
      carp(CARP_FATAL, "Failed to create output directory %s", 
           output_directory);
    }
  }

  // get ms2 file for naming result file
  //TODO change to output filename as argument, force .csm extension
  //     add _decoy1.csm
  //char* base_filename = get_string_parameter_pointer("ms2 file");
  const char* ms2_filename = get_string_parameter_pointer("ms2 file");
  //char** filename_path_array = parse_filename_path(base_filename);
  char** filename_path_array = 
    parse_filename_path_extension(ms2_filename, ".ms2");
  if( filename_path_array[1] == NULL ){
    filename_path_array[1] = ".";
  }

  carp(CARP_DEBUG, "Base filename is %s and path is %s", 
       filename_path_array[0], filename_path_array[1]);

  char* filename_template = get_full_filename(output_directory, 
                                              filename_path_array[0]);

  //create target file
  BOOLEAN_T overwrite = get_boolean_parameter("overwrite");

  for(file_idx=0; file_idx<total_files; file_idx++){

    char* psm_filename = generate_psm_filename(file_idx);

    file_handle_array[file_idx] = create_file_in_path(psm_filename,
                                                      output_directory,
                                                      overwrite); 
    //check for error
    if( file_handle_array[file_idx] == NULL ){//||
      carp(CARP_FATAL, "Could not create psm file %s", psm_filename);
    }
    //rename this, just for a quick fix
    free(filename_template);
    filename_template = get_full_filename(output_directory, psm_filename);
    chmod(filename_template, 0664);

    // clean up
    free(psm_filename);
    
  }// next file

  // clean up 
  free(filename_path_array[0]);
  if( *filename_path_array[1] != '.' ){
    free(filename_path_array[1]);
  }
  free(filename_path_array);
  free(filename_template);

  return file_handle_array;

}

/**
 * \brief Serialize the PSM features to output file up to 'top_match'
 * number of top peptides from the match_collection.
 *
 * \details  First serialize the spectrum info of the match collection
 * then  iterate over matches and serialize the structs
 *
 * <int: charge state of the spectrum>
 * <int: Total match objects in the match_collection>
 * <float: delta_cn>
 * <float: ln_delta_cn>
 * <float: ln_experiment_size>
 * <BOOLEAN_T: had the score type been scored?>* - for all score types
 * <MATCH: serialized match struct>* <--serialize top_match match structs 
 *
 * \returns TRUE, if sucessfully serializes the PSMs, else FALSE 
 * \callgraph
 */
BOOLEAN_T serialize_psm_features(
  MATCH_COLLECTION_T* match_collection, ///< working match collection -in
  FILE* output,               ///< output file handle -out
  int top_match,              ///< number of top match to serialize -in
  SCORER_TYPE_T prelim_score, ///< the preliminary score to report -in
  SCORER_TYPE_T main_score    ///<  the main score to report -in
  )
{
  if( match_collection == NULL || output == NULL ){
    carp(CARP_FATAL, "Cannot serialize psm features with NULL match collection and/or output file.");
  }
  MATCH_T* match = NULL;
  
  // create match iterator 
  // TRUE tells iterator to return matches in sorted order of main_score type
  MATCH_ITERATOR_T* match_iterator = 
    new_match_iterator(match_collection, main_score, TRUE);
  
  FLOAT_T delta_cn =  get_match_collection_delta_cn(match_collection);
  FLOAT_T ln_delta_cn = logf(delta_cn);
  // FIXME (BF 16-Sep-08): log(delta_cn) isn't even a feature in percolator
  if( delta_cn == 0 ){
    // this value makes it the same as what is in the smoke test
    //ln_delta_cn = -13.8155;
    ln_delta_cn = 0;
  }
  FLOAT_T ln_experiment_size = logf(match_collection->experiment_size);

  // spectrum specific features
  // first, serialize the spectrum info of the match collection  
  // the charge of the spectrum
  
  myfwrite(&(match_collection->charge), sizeof(int), 1, output); 
  myfwrite(&(match_collection->match_total), sizeof(int), 1, output);
  myfwrite(&delta_cn, sizeof(FLOAT_T), 1, output);
  myfwrite(&ln_delta_cn, sizeof(FLOAT_T), 1, output);
  myfwrite(&ln_experiment_size, sizeof(FLOAT_T), 1, output);

  // serialize each boolean for scored type 
  int score_type_idx;
  // We don't want to change the CSM files contents so we omit q-ranker scores
  // which were added to Crux after the CSM file format had been established.
  int score_type_max = _SCORE_TYPE_NUM - 2;
  for(score_type_idx=0; score_type_idx < score_type_max; ++score_type_idx){
    myfwrite(&(match_collection->scored_type[score_type_idx]), 
        sizeof(BOOLEAN_T), 1, output);
  }
  
  // second, iterate over matches and serialize them
  int match_count = 0;
  while(match_iterator_has_next(match_iterator)){
    ++match_count;
    match = match_iterator_next(match_iterator);        
    
    // FIXME
    prelim_score = prelim_score;
    
    // serialize matches
    carp(CARP_DETAILED_DEBUG, "About to serialize match %d, z %d, null %d",
         get_spectrum_first_scan(get_match_spectrum(match)),
         get_match_charge(match),
         get_match_null_peptide(match));

    serialize_match(match, output); // FIXME main, preliminary type
    
    // print only up to max_rank_result of the matches
    if(match_count >= top_match){
      break;
    }
  }
  
  free_match_iterator(match_iterator);
  
  return TRUE;
}

void print_sqt_header(
 FILE* output, 
 const char* type, 
 int num_proteins, 
 BOOLEAN_T is_analysis){  // for analyze-matches look at algorithm param
  if( output == NULL ){
    return;
  }

  time_t hold_time;
  hold_time = time(0);

  BOOLEAN_T decoy = FALSE;
  if( strcmp(type, "decoy") == 0 ){
    decoy = TRUE;
  }

  fprintf(output, "H\tSQTGenerator Crux\n");
  fprintf(output, "H\tSQTGeneratorVersion 1.0\n");
  fprintf(output, "H\tComment Crux was written by...\n");
  fprintf(output, "H\tComment ref...\n");
  fprintf(output, "H\tStartTime\t%s", ctime(&hold_time));
  fprintf(output, "H\tEndTime                               \n");

  char* database = get_string_parameter("protein input");
  BOOLEAN_T use_index = is_directory(database);

  if( use_index == TRUE ){
    char* fasta_name  = get_index_binary_fasta_name(database);
    free(database);
    database = fasta_name;
  }
  fprintf(output, "H\tDatabase\t%s\n", database);
  free(database);

  if(decoy){
  fprintf(output, "H\tComment\tDatabase shuffled; these are decoy matches\n");
  }
  fprintf(output, "H\tDBSeqLength\t?\n");
  fprintf(output, "H\tDBLocusCount\t%d\n", num_proteins);

  MASS_TYPE_T mass_type = get_mass_type_parameter("isotopic-mass");
  char temp_str[64];
  mass_type_to_string(mass_type, temp_str);
  fprintf(output, "H\tPrecursorMasses\t%s\n", temp_str);
  
  mass_type = get_mass_type_parameter("fragment-mass");
  mass_type_to_string(mass_type, temp_str);
  fprintf(output, "H\tFragmentMasses\t%s\n", temp_str); //?????????

  double tol = get_double_parameter("mass-window");
  fprintf(output, "H\tAlg-PreMasTol\t%.1f\n",tol);
  fprintf(output, "H\tAlg-FragMassTol\t%.2f\n", 
          get_double_parameter("ion-tolerance"));
  fprintf(output, "H\tAlg-XCorrMode\t0\n");

  SCORER_TYPE_T score = get_scorer_type_parameter("prelim-score-type");
  scorer_type_to_string(score, temp_str);
  fprintf(output, "H\tComment\tpreliminary algorithm %s\n", temp_str);

  score = get_scorer_type_parameter("score-type");
  scorer_type_to_string(score, temp_str);
  fprintf(output, "H\tComment\tfinal algorithm %s\n", temp_str);

  int aa = 0;
  char aa_str[2];
  aa_str[1] = '\0';
  int alphabet_size = (int)'A' + ((int)'Z'-(int)'A');
  MASS_TYPE_T isotopic_type = get_mass_type_parameter("isotopic-mass");

  for(aa = (int)'A'; aa < alphabet_size -1; aa++){
    aa_str[0] = (char)aa;
    double mod = get_double_parameter(aa_str);
    if( mod != 0 ){
      //      double mass = mod + get_mass_amino_acid(aa, isotopic_type);
      double mass = get_mass_amino_acid(aa, isotopic_type);
      fprintf(output, "H\tStaticMod\t%s=%.3f\n", aa_str, mass);
    }
  }
  // print dynamic mods, if any
  // format DiffMod <AAs><symbol>=<mass change>
  AA_MOD_T** aa_mod_list = NULL;
  int num_mods = get_all_aa_mod_list(&aa_mod_list);
  int mod_idx = 0;
  for(mod_idx = 0; mod_idx < num_mods; mod_idx++){
    
    AA_MOD_T* aamod = aa_mod_list[mod_idx];
    char* aa_list_str = aa_mod_get_aa_list_string(aamod);
    char aa_symbol = aa_mod_get_symbol(aamod);
    double mass_dif = aa_mod_get_mass_change(aamod);

    fprintf(output, "H\tDiffMod\t%s%c=%+.2f\n", aa_list_str, 
            aa_symbol, mass_dif);
    free(aa_list_str);
  }
  num_mods = get_c_mod_list(&aa_mod_list);
  for(mod_idx = 0; mod_idx < num_mods; mod_idx++){
    AA_MOD_T* aamod = aa_mod_list[mod_idx];
    char aa_symbol = aa_mod_get_symbol(aamod);

    fprintf(output, "H\tComment\tMod %c is a C-terminal modification\n",
            aa_symbol);
  }

  num_mods = get_n_mod_list(&aa_mod_list);
  for(mod_idx = 0; mod_idx < num_mods; mod_idx++){
    AA_MOD_T* aamod = aa_mod_list[mod_idx];
    char aa_symbol = aa_mod_get_symbol(aamod);

    fprintf(output, "H\tComment\tMod %c is a N-terminal modification\n",
            aa_symbol);
  }



  //for letters in alphabet
  //  double mod = get_double_parameter(letter);
  //  if mod != 0
  //     double mass = mod + getmass(letter);
  //     fprintf(output, "H\tStaticMod\t%s=%.3f\n", letter, mass);
  //  fprintf(output, "H\tStaticMod\tC=160.139\n");
  fprintf(output, "H\tAlg-DisplayTop\t%d\n", 
          //          get_int_parameter("max-sqt-result")); 
          get_int_parameter("top-match")); 
  // this is not correct for an sqt from analzyed matches

  //PEPTIDE_TYPE_T cleavages = get_peptide_type_parameter("cleavages");
  ENZYME_T enzyme = get_enzyme_type_parameter("enzyme");
  DIGEST_T digestion = get_digest_type_parameter("digestion");
  //peptide_type_to_string(cleavages, temp_str);
  char* enz_str = enzyme_type_to_string(enzyme);
  char* dig_str = digest_type_to_string(digestion);
  char custom_str[SMALL_BUFFER];
  if( enzyme == CUSTOM_ENZYME){
    char* rule = get_string_parameter("custom-enzyme");
    sprintf(custom_str, ", custom pattern: %s", rule);
  }else{
    custom_str[0] = 0;
  }
  fprintf(output, "H\tEnzymeSpec\t%s-%s%s\n", enz_str, dig_str, custom_str);
  free(enz_str);
  free(dig_str);

  // write a comment that says what the scores are
  fprintf(output, "H\tLine fields: S, scan number, scan number,"
          "charge, 0, precursor mass, 0, 0, number of matches\n");

  // fancy logic for printing the scores. see match.c:print_match_sqt

  SCORER_TYPE_T main_score = get_scorer_type_parameter("score-type");
  SCORER_TYPE_T other_score = get_scorer_type_parameter("prelim-score-type");
  ALGORITHM_TYPE_T analysis_score = get_algorithm_type_parameter("algorithm");
  BOOLEAN_T pvalues = get_boolean_parameter("compute-p-values");
  if( is_analysis == TRUE && analysis_score == PERCOLATOR_ALGORITHM){
    main_score = PERCOLATOR_SCORE; 
    other_score = Q_VALUE;
  }else if( is_analysis == TRUE && analysis_score == QRANKER_ALGORITHM ){
    main_score = QRANKER_SCORE; 
    other_score = QRANKER_Q_VALUE;
  }else if( is_analysis == TRUE && analysis_score == QVALUE_ALGORITHM ){
    main_score = LOGP_QVALUE_WEIBULL_XCORR;
  }else if( pvalues == TRUE ){
    main_score = LOGP_BONF_WEIBULL_XCORR;
  }

  char main_score_str[64];
  scorer_type_to_string(main_score, main_score_str);
  char other_score_str[64];
  scorer_type_to_string(other_score, other_score_str);

  // ranks are always xcorr and sp
  // main/other scores from search are...xcorr/sp (OK as is)
  // ...p-val/xcorr
  if( main_score == LOGP_BONF_WEIBULL_XCORR ){
    strcpy(main_score_str, "-log(p-value)");
    strcpy(other_score_str, "xcorr");
  }// main/other scores from analyze are perc/q-val (OK as is)
   // q-val/xcorr
  if( main_score == LOGP_QVALUE_WEIBULL_XCORR ){
    strcpy(main_score_str, "q-value");  // to be changed to curve-fit-q-value
    strcpy(other_score_str, "xcorr");
  }

  fprintf(output, "H\tLine fields: M, rank by xcorr score, rank by sp score, "
          "peptide mass, deltaCn, %s score, %s score, number ions matched, "
          "total ions compared, sequence\n", main_score_str, other_score_str);
}

void print_tab_header(FILE* output){

  if( output == NULL ){
    return;
  }

  fprintf(
    output, 
    "scan\t"
    "charge\t"
    "spectrum precursor m/z\t"
    "spectrum neutral mass\t"
    "peptide mass\t"
    "delta_cn\t"
    "sp score\t"
    "sp rank\t"
    "xcorr score\t"
    "xcorr rank\t"
    "p-value\t"
    "Weibull est. q-value\t"
    "decoy q-value (xcorr)\t"
    "decoy q-value (p-value)\t"
    "percolator score\t"
    "percolator rank\t"
    "percolator q-value\t"
    "q-ranker score\t"
    "q-ranker q-value\t"
    "b/y ions matched\t"
    "b/y ions total\t"
    "matches/spectrum\t"
    "sequence\t"
    "cleavage type\t"
    "protein id\t"
    "flanking aa\t"
    "unshuffled sequence\t"
    "eta\t"
    "beta\t"
    "shift\t"
    "corr\n"
  );
}

/**
 * \brief Print the psm features to file in sqt format.
 *
 * Prints one S line, 'top_match' M lines, and one locus line for each
 * peptide source of each M line.
 * Assumes one spectrum per match collection.  Could get top_match,
 * score types from parameter.c.  Could get spectrum from first match.
 *\returns TRUE, if sucessfully print sqt format of the PSMs, else FALSE 
 */
BOOLEAN_T print_match_collection_sqt(
  FILE* output,                  ///< the output file -out
  int top_match,                 ///< the top matches to output -in
  MATCH_COLLECTION_T* match_collection,
  ///< the match_collection to print sqt -in
  SPECTRUM_T* spectrum,          ///< the spectrum to print sqt -in
  SCORER_TYPE_T prelim_score,    ///< the preliminary score to report -in
  SCORER_TYPE_T main_score       ///< the main score to report -in
  )
{

  if( output == NULL || match_collection == NULL || spectrum == NULL ){
    return FALSE;
  }
  time_t hold_time;
  hold_time = time(0);
  int charge = match_collection->charge; 
  int num_matches = match_collection->experiment_size;

  // If we calculated p-values, change which scores get printed
  // since this is really only valid for xcorr...
  assert( main_score == XCORR );
  BOOLEAN_T pvalues = get_boolean_parameter("compute-p-values");
  SCORER_TYPE_T score_to_print_first = main_score;
  SCORER_TYPE_T score_to_print_second = prelim_score;
  if( pvalues ){
    score_to_print_second = score_to_print_first;
    score_to_print_first = LOGP_BONF_WEIBULL_XCORR; // soon to be P_VALUES
  }

  // calculate delta_cn and populate fields in the matches
  calculate_delta_cn(match_collection);

  // First, print spectrum info
  print_spectrum_sqt(spectrum, output, num_matches, charge);
  
  MATCH_T* match = NULL;
  
  // create match iterator
  // TRUE: return match in sorted order of main_score type
  MATCH_ITERATOR_T* match_iterator = 
    new_match_iterator(match_collection, main_score, TRUE);
  
  // Second, iterate over matches, prints M and L lines
  while(match_iterator_has_next(match_iterator)){
    match = match_iterator_next(match_iterator);    

    // print only up to max_rank_result of the matches
    if( get_match_rank(match, main_score) > top_match ){
      break;
    }// else

    print_match_sqt(match, output, 
                    score_to_print_first, score_to_print_second);

  }// next match
  
  free_match_iterator(match_iterator);
  
  return TRUE;
}

/**
 * \brief Print the psm features to file in tab delimited format.
 *
 * Matches will be sorted by main_score and the ranks of those scores
 * will be used to determine how many matches are printed for each
 * spectrum.
 * \returns TRUE, if sucessfully print tab-delimited format of the
 * PSMs, else FALSE  
 */
BOOLEAN_T print_match_collection_tab_delimited(
  FILE* output,                  ///< the output file -out
  int top_match,                 ///< the top matches to output -in
  MATCH_COLLECTION_T* match_collection,
  ///< the match_collection to print sqt -in
  SPECTRUM_T* spectrum,          ///< the spectrum to print sqt -in
  SCORER_TYPE_T main_score       ///< the main score to report -in
  )
{

  if( output == NULL || match_collection == NULL || spectrum == NULL ){
    return FALSE;
  }
  time_t hold_time;
  hold_time = time(0);
  int charge = match_collection->charge; 
  int num_matches = match_collection->experiment_size;
  int scan_num = get_spectrum_first_scan(spectrum);
  FLOAT_T spectrum_neutral_mass = get_spectrum_neutral_mass(spectrum, charge);
  FLOAT_T spectrum_precursor_mz = get_spectrum_precursor_mz(spectrum);

  // calculate delta_cn and populate fields in the matches
  calculate_delta_cn(match_collection);

  MATCH_T* match = NULL;
  
  // create match iterator
  // TRUE: return match in sorted order of main_score type
  MATCH_ITERATOR_T* match_iterator = 
    new_match_iterator(match_collection, main_score, TRUE);
  
  // iterate over matches
  while(match_iterator_has_next(match_iterator)){
    match = match_iterator_next(match_iterator);    

    // print only up to max_rank_result of the matches
    if( get_match_rank(match, main_score) > top_match ){
      break;
    }// else

    print_match_tab(match_collection, match, output, scan_num, 
		    spectrum_precursor_mz, 
                    spectrum_neutral_mass, num_matches, charge, 
                    match_collection->scored_type);

  }// next match
  
  free_match_iterator(match_iterator);
  
  return TRUE;
}

/**
 * Print the calibration parameters eta, beta, shift and correlation
 * with tabs between.
 */
void print_calibration_parameters(
  MATCH_COLLECTION_T* my_collection, ///< The collection -in
  FILE* output ///< The output file -in
  )
{
  fprintf(output,
	  "\t%g\t%g\t%g\t%g",
	  my_collection->eta,
	  my_collection->beta,
	  my_collection->shift,
	  my_collection->correlation);
}

/**
 * match_iterator routines!
 *
 */

/**
 * create a new memory allocated match iterator, which iterates over
 * match iterator only one iterator is allowed to be instantiated per
 * match collection at a time 
 *\returns a new memory allocated match iterator
 */
MATCH_ITERATOR_T* new_match_iterator(
  MATCH_COLLECTION_T* match_collection,
  ///< the match collection to iterate -out
  SCORER_TYPE_T score_type,
  ///< the score type to iterate (LOGP_EXP_SP, XCORR) -in
  BOOLEAN_T sort_match  ///< should I return the match in sorted order?
  )
{
  // TODO (BF 06-Feb-08): Could we pass back an iterator with has_next==False
  if (match_collection == NULL){
    carp(CARP_FATAL, "Null match collection passed to match iterator");
  }
  // is there an existing iterator?
  if(match_collection->iterator_lock){
    carp(CARP_FATAL, 
         "Can only have one match iterator instantiated at a time");
  }
  
  // has the score type been populated in match collection?
  if(!match_collection->scored_type[score_type]){
    char score_str[64];
    scorer_type_to_string(score_type, score_str);
    carp(CARP_ERROR, "New match iterator for score type %s.", score_str);
    carp(CARP_FATAL, 
         "The match collection has not been scored for request score type.");
  }
  
  // allocate a new match iterator
  MATCH_ITERATOR_T* match_iterator = 
    (MATCH_ITERATOR_T*)mycalloc(1, sizeof(MATCH_ITERATOR_T));
  
  // set items
  match_iterator->match_collection = match_collection;
  match_iterator->match_mode = score_type;
  match_iterator->match_idx = 0;
  match_iterator->match_total = match_collection->match_total;

  // only sort if requested and match collection is not already sorted
  if(sort_match && (match_collection->last_sorted != score_type 
  /*|| (match_collection->last_sorted == SP && score_type == LOGP_EXP_SP)*/)){

    if((score_type == LOGP_EXP_SP //|| score_type == LOGP_BONF_EXP_SP ||
        //        score_type == LOGP_WEIBULL_SP
 || score_type == LOGP_BONF_WEIBULL_SP)
       &&
       match_collection->last_sorted == SP){
      // No need to sort, since the score_type has same rank as SP      
    }
    
    //else if((score_type == LOGP_EVD_XCORR || score_type ==LOGP_BONF_EVD_XCORR)
    else if(score_type ==LOGP_BONF_EVD_XCORR
            && match_collection->last_sorted == XCORR){
      // No need to sort, since the score_type has same rank as XCORR
    }
    else if((score_type == Q_VALUE) &&
            match_collection->last_sorted == PERCOLATOR_SCORE){
      // No need to sort, the score_type has same rank as PERCOLATOR_SCORE
    }
    else if((score_type == QRANKER_Q_VALUE) &&
            match_collection->last_sorted == QRANKER_SCORE){
      // No need to sort, the score_type has same rank as QRANKER_SCORE
    }
    // sort match collection by score type
    else if(!sort_match_collection(match_collection, score_type)){
      carp(CARP_FATAL, "failed to sort match collection");
    }
  }

  // ok lock up match collection
  match_collection->iterator_lock = TRUE;
  
  return match_iterator;
}

/**
 * \brief Create a match iterator to return matches from a collection
 * grouped by spectrum and sorted by given score type.
 *
 * \returns A heap-allocated match iterator.
 */
MATCH_ITERATOR_T* new_match_iterator_spectrum_sorted(
  MATCH_COLLECTION_T* match_collection,  ///< for iteration -in
  SCORER_TYPE_T scorer ///< the score type to sort by -in
){

  MATCH_ITERATOR_T* match_iterator = 
    (MATCH_ITERATOR_T*)mycalloc(1, sizeof(MATCH_ITERATOR_T));

  // set up fields
  match_iterator->match_collection = match_collection;
  match_iterator->match_mode = scorer;
  match_iterator->match_idx = 0;
  match_iterator->match_total = match_collection->match_total;

  spectrum_sort_match_collection(match_collection, scorer);

  match_collection->iterator_lock = TRUE;

  return match_iterator;
}

/**
 * Does the match_iterator have another match struct to return?
 *\returns TRUE, if match iter has a next match, else False
 */
BOOLEAN_T match_iterator_has_next(
  MATCH_ITERATOR_T* match_iterator ///< the working  match iterator -in
  )
{
  return (match_iterator->match_idx < match_iterator->match_total);
}

/**
 * return the next match struct!
 *\returns the match in decreasing score order for the match_mode(SCORER_TYPE_T)
 */
MATCH_T* match_iterator_next(
  MATCH_ITERATOR_T* match_iterator ///< the working match iterator -in
  )
{
  return match_iterator->match_collection->match[match_iterator->match_idx++];
}

/**
 * free the memory allocated iterator
 */
void free_match_iterator(
  MATCH_ITERATOR_T* match_iterator ///< the match iterator to free
  )
{
  // free iterator
  if (match_iterator != NULL){
    if (match_iterator->match_collection != NULL){
      match_iterator->match_collection->iterator_lock = FALSE;
    }
    free(match_iterator);
  }
}

/*
 * Copied from spectrum_collection::serialize_header
 * uses values from paramter.c rather than taking as arguments
 */
/**
 * \brief Write header information to each file in the given array of
 * filehandles. Writes the number of matches per spectra and a place
 * holder is written for the total number of spectra.  The array of
 * modifications kept by parameter.c and the number of modications in
 * that array is also written.
 */
void serialize_headers(FILE** psm_file_array){

  if( *psm_file_array == NULL ){
    return;
  }

  // remove this
  int num_spectrum_features = 0; //obsolete?

  // get values from parameter.c
  int num_charged_spectra = -1;  //this is set later
  int matches_per_spectrum = get_int_parameter("top-match");
  char* filename = get_string_parameter("protein input");
  char* protein_file = parse_filename(filename);
  //filename = get_string_parameter_pointer("ms2 file");
  //filename = get_string_parameter("ms2 file");
  //char* ms2_file = parse_filename(filename);
  free(filename);
           
  AA_MOD_T** list_of_mods = NULL;
  int num_mods = get_all_aa_mod_list(&list_of_mods);

  // TODO: should this also write the ms2 filename???

  //write values to files
  int total_files = 1 + get_int_parameter("num-decoy-files");
  carp(CARP_DETAILED_DEBUG, "Serializing headers in %i files", total_files);
  carp(CARP_DETAILED_DEBUG, "%i matches per spec", matches_per_spectrum);
  int i=0;
  for(i=0; i<total_files; i++){
    fwrite(&(num_charged_spectra), sizeof(int), 1, psm_file_array[i]);
    fwrite(&(num_spectrum_features), sizeof(int), 1, psm_file_array[i]);
    fwrite(&(matches_per_spectrum), sizeof(int), 1, psm_file_array[i]);

    fwrite(&num_mods, sizeof(int), 1, psm_file_array[i]);
    // this a list of pointers to mods, write each one
    int mod_idx = 0;
    for(mod_idx = 0; mod_idx<num_mods; mod_idx++){
    //fwrite(list_of_mods[mod_idx], get_aa_mod_sizeof(), 1, psm_file_array[i]);
      serialize_aa_mod(list_of_mods[mod_idx], psm_file_array[i]);
    }
  }
  
  free(protein_file);
  //free(ms2_file);

}

/**
 * \brief Read in the header information from a cms file.  Return
 * FALSE if file appears to be corrupted or if mod information does
 * not mat parameter.c
 * \returns TRUE if header was successfully parsed, else FALSE.
 */
BOOLEAN_T parse_csm_header
 (FILE* file,
  int* total_spectra,
  int* num_top_match)
{

  // get number of spectra serialized in the file
  if(fread(total_spectra, (sizeof(int)), 1, file) != 1){
    carp(CARP_ERROR, "Could not read spectrum count from csm file header.");
    return FALSE;
  }
  carp(CARP_DETAILED_DEBUG, "There are %i spectra in the result file", 
       *total_spectra);
  if( *total_spectra < 0 ){ // value initialized to -1
    carp(CARP_ERROR, "Header of csm file incomplete, spectrum count missing. "
         "Did the search run without error?");
    return FALSE;
  }

  // FIXME unused feature, just set to 0
  int num_spectrum_features = 555;
  // get number of spectra features serialized in the file
  if(fread(&num_spectrum_features, (sizeof(int)), 1, file) != 1){
    carp(CARP_ERROR, 
         "Serialized file corrupted, incorrect number of spectrum features");
    return FALSE;
  }
  
  carp(CARP_DETAILED_DEBUG, "There are %i spectrum features", 
       num_spectrum_features);

  // get number top ranked peptides serialized
  if(fread(num_top_match, (sizeof(int)), 1, file) != 1){
    carp(CARP_ERROR, 
         "Serialized file corrupted, incorrect number of top match");  
    return FALSE;
  }
  carp(CARP_DETAILED_DEBUG, "There are %i top matches", *num_top_match);

  // modification specific information
  int num_mods = -1;
  fread(&num_mods, sizeof(int), 1, file);
  carp(CARP_DETAILED_DEBUG, "There are %i aa mods", num_mods);

  AA_MOD_T* file_mod_list[MAX_AA_MODS];
  int mod_idx = 0;
  for(mod_idx = 0; mod_idx<num_mods; mod_idx++){
    AA_MOD_T* cur_mod = new_aa_mod(mod_idx);
    //fread(cur_mod, get_aa_mod_sizeof(), 1, file);
    parse_aa_mod(cur_mod, file);
    //print_a_mod(cur_mod);
    file_mod_list[mod_idx] = cur_mod;
  }

  if(! compare_mods(file_mod_list, num_mods) ){
    carp(CARP_ERROR, "Modification parameters do not match those in " \
                     "the csm file.");
    return  FALSE;
  }

  return TRUE;
}

/**
 * \brief Print the given match collection for several spectra to
 * tab-delimited files only.  Takes the spectrum information from the
 * matches in the collection.  At least for now, prints all matches in
 * the collection rather than limiting by top-match parameter.  Uses
 * SP as preliminary score and XCORR as main score.
 */
void print_matches_multi_spectra
(MATCH_COLLECTION_T* match_collection, 
 FILE* tab_file, 
 FILE* decoy_tab_file){


  carp(CARP_DETAILED_DEBUG, "Writing matches to file");

  // if file location is target (i.e. tdc=T), print all to target
  FILE* decoy_file = decoy_tab_file;
  if( get_boolean_parameter("tdc") == TRUE ){
    decoy_file = tab_file;
  }

  // for each match, get spectrum info, determine if decoy, print
  int match_idx = 0;
  int num_matches = match_collection->match_total;
  for(match_idx = 0; match_idx < num_matches; match_idx++){
    MATCH_T* cur_match = match_collection->match[match_idx];
    BOOLEAN_T is_decoy = get_match_null_peptide(cur_match);
    SPECTRUM_T* spectrum = get_match_spectrum(cur_match);
    int scan_num = get_spectrum_first_scan(spectrum);
    FLOAT_T mz = get_spectrum_precursor_mz(spectrum);
    int charge = get_match_charge(cur_match);
    FLOAT_T spec_mass = get_spectrum_neutral_mass(spectrum, charge);
    FLOAT_T num_psm_per_spec = get_match_ln_experiment_size(cur_match);
    num_psm_per_spec = expf(num_psm_per_spec) + 0.5; // round to nearest int

    if( is_decoy ){
      print_match_tab(match_collection, cur_match, decoy_file, scan_num, mz, 
                      spec_mass, (int)num_psm_per_spec, charge, match_collection->scored_type );
    }
    else{
      print_match_tab(match_collection, cur_match, tab_file, scan_num, mz,
                      spec_mass, (int)num_psm_per_spec, charge, match_collection->scored_type );
    }

  }

}

/*******************************************
 * match_collection post_process extension
 ******************************************/

/**
 * \brief Creates a new match_collection from the PSM iterator.
 *
 * Used in the post_processing extension.  Also used by
 * setup_match_collection_iterator which is called by next to find,
 * open, and parse the next psm file(s) to process.  If there are
 * multiple target psm files, it reads in all of them when set_type is
 * 0 and puts them all into one match_collection. 
 *\returns A heap allocated match_collection.
 */
MATCH_COLLECTION_T* new_match_collection_psm_output(
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator, 
    ///< the working match_collection_iterator -in
  SET_TYPE_T set_type  
    ///< what set of match collection are we creating? (TARGET, DECOY1~3) -in 
  )
{ 
  struct dirent* directory_entry = NULL;
  char* file_in_dir = NULL;
  FILE* result_file = NULL;
  char suffix[25];

  carp(CARP_DEBUG, "Calling new_match_collection_psm_output");
  DATABASE_T* database = match_collection_iterator->database;
  
  // allocate match_collection object
  MATCH_COLLECTION_T* match_collection = allocate_match_collection();

  // set this as a post_process match collection
  match_collection->post_process_collection = TRUE;
  
  // the protein counter size, create protein counter
  match_collection->post_protein_counter_size 
   = get_database_num_proteins(database);
  match_collection->post_protein_counter 
   = (int*)mycalloc(match_collection->post_protein_counter_size, sizeof(int));
  match_collection->post_protein_peptide_counter 
   = (int*)mycalloc(match_collection->post_protein_counter_size, sizeof(int));

  // create hash table for peptides
  // Set initial capacity to protein count.
  match_collection->post_hash 
    = new_hash(match_collection->post_protein_counter_size);
  
  // set the suffix of the serialized file to parse
  // Also, tag if match_collection type is null_peptide_collection

  if(set_type == SET_TARGET){
    sprintf(suffix, ".target.csm");
    match_collection->null_peptide_collection = FALSE;
  }
  else{
    sprintf(suffix, ".decoy-%d.csm", (int)set_type);
    match_collection->null_peptide_collection = TRUE;
  }
  
  carp(CARP_DEBUG, "Set type is %d and suffix is %s", (int)set_type, suffix);
  BOOLEAN_T found_file = FALSE;
  // iterate over all PSM files in directory to find the one to read
  while((directory_entry 
            = readdir(match_collection_iterator->working_directory))){

    //carp(CARP_DETAILED_DEBUG, "Next file is %s", directory_entry->d_name);

    // skip over any file not ending in .csm
    if( !suffix_compare(directory_entry->d_name, ".csm") ) {
      continue;
    }

    // it's the right file if ...
    //      type is target and ends in "target.cms"
    //      type is SET_DECOY1 and ends in "decoy.csm"
    //       type is t and ends in "decoy-t.csm"
    if( set_type == SET_TARGET && 
        suffix_compare(directory_entry->d_name, "target.csm") ){
      found_file = TRUE;
      break;
    } else if( set_type == SET_DECOY1 && 
              suffix_compare(directory_entry->d_name, "decoy.csm") ){
      found_file = TRUE;
      break;
    } else if( suffix_compare(directory_entry->d_name, suffix) ){
      found_file = TRUE;
      break;
    }
  }

  if( ! found_file ){
    carp(CARP_ERROR, "Could not find file ending in '%s'.", suffix);
  }

  file_in_dir = get_full_filename(match_collection_iterator->directory_name, 
                                  directory_entry->d_name);
  
  carp(CARP_INFO, "Getting PSMs from %s", file_in_dir);
  result_file = fopen(file_in_dir, "r");
  if( access(file_in_dir, R_OK)){
    carp(CARP_FATAL, "Cannot read from psm file '%s'", file_in_dir);
  }
  // add all the match objects from result_file
  extend_match_collection(match_collection, database, result_file);
  carp(CARP_DETAILED_DEBUG, "Extended match collection " );
  fclose(result_file);
  free(file_in_dir);
  carp(CARP_DETAILED_DEBUG, "Finished file.");
  
  return match_collection;
}


/**
 * parse all the match objects and add to match collection
 *\returns TRUE, if successfully parse all PSMs in result_file, else FALSE
 */
BOOLEAN_T extend_match_collection(
  MATCH_COLLECTION_T* match_collection, ///< match collection to extend -out
  DATABASE_T* database, ///< the database holding the peptides -in
  FILE* result_file   ///< the result file to parse PSMs -in
  )
{
  int total_spectra = 0;
  int match_idx = 0;
  int spectrum_idx = 0;
  int charge = 0;
  MATCH_T* match = NULL;
  int num_top_match = 0;
  //  int num_spectrum_features = 0;
  FLOAT_T delta_cn =  0;
  FLOAT_T ln_delta_cn = 0;
  FLOAT_T ln_experiment_size = 0;
  int match_total_of_serialized_collection = 0;
  int score_type_idx = 0;
  BOOLEAN_T type_scored = FALSE;

  // only for post_process_collections
  if(!match_collection->post_process_collection){
    carp(CARP_ERROR, "Must be a post process match collection to extend.");
    return FALSE;
  }
  
  // read in file specific info
  if(!  parse_csm_header(result_file, &total_spectra, &num_top_match)){
    carp(CARP_FATAL, "Error reading csm header.");
  }
  carp(CARP_DETAILED_DEBUG, "There are %i top matches", num_top_match);

  // FIXME
  // could parse fasta file and ms2 file
  
  // now iterate over all spectra serialized
  for(spectrum_idx = 0; spectrum_idx < total_spectra; ++spectrum_idx){
    /*** get all spectrum specific features ****/
    
    int chars_read = fread(&charge, (sizeof(int)), 1, result_file);
    carp(CARP_DETAILED_DEBUG, "Read %i characters, charge is %i",
         chars_read, charge);

    // get serialized match_total
    chars_read = fread(&match_total_of_serialized_collection, (sizeof(int)),
                       1, result_file);
    carp(CARP_DETAILED_DEBUG, "Read %i characters, match total is %i",
         chars_read, match_total_of_serialized_collection);
      
    // get delta_cn value
    if(fread(&delta_cn, (sizeof(FLOAT_T)), 1, result_file) != 1){
      carp(CARP_ERROR, 
       "Serialized file corrupted, incorrect delta cn value for top match");  
      return FALSE;
    }
    
    // get ln_delta_cn value
    if(fread(&ln_delta_cn, (sizeof(FLOAT_T)), 1, result_file) != 1){
      carp(CARP_ERROR, 
    "Serialized file corrupted, incorrect ln_delta cn value for top match");  
      return FALSE;
    }
    
    // get ln_experiment_size
    if(fread(&ln_experiment_size, (sizeof(FLOAT_T)), 1, result_file) != 1){
      carp(CARP_ERROR, "Serialized file corrupted, incorrect "
           "ln_experiment_size cn value for top match");  
      return FALSE;
    }
    
    // Read each boolean for scored type 
    // parse all boolean indicators for scored match object
    // We don't want to change the CSM files contents so we omit q-ranker scores
    // which were added to Crux after the CSM file format had been established.
    int score_type_max = _SCORE_TYPE_NUM - 2;
    for(score_type_idx=0; score_type_idx < score_type_max; ++score_type_idx){
      fread(&(type_scored), sizeof(BOOLEAN_T), 1, result_file);
      
      // if this is the first time extending the match collection
      // set scored boolean values
      if(!match_collection->post_scored_type_set){
        match_collection->scored_type[score_type_idx] = type_scored;
      }
      else{
        // if boolean values already set, look for conflicting scored types 
        // this is overzealous since some pvalues could not be scored
        /*
        if(match_collection->scored_type[score_type_idx] != type_scored){
          carp(CARP_ERROR, "Serialized match objects has not been scored "
               "as other match objects");
        }
        */
      }
      // now once we are done with setting scored type
      // set match collection status as set!
      if(!match_collection->post_scored_type_set &&
         score_type_idx == (score_type_max-1)){
        match_collection->post_scored_type_set = TRUE;
      }
    }
    
    // now iterate over all 
    for(match_idx = 0; match_idx < num_top_match; ++match_idx){
      // break if there are no match objects serialized
      //      if(match_total_of_serialized_collection <= 0){
      if(match_total_of_serialized_collection <= match_idx){
        break;
      }
      
      carp(CARP_DETAILED_DEBUG, "Reading match %i", match_idx);
      // parse match object
      if((match = parse_match(result_file, database))==NULL){
        carp(CARP_ERROR, "Failed to parse serialized PSM match");
        return FALSE;
      }
      
      // set all spectrum specific features to parsed match
      set_match_charge(match, charge);
      set_match_delta_cn(match, delta_cn);
      set_match_ln_delta_cn(match, ln_delta_cn);
      set_match_ln_experiment_size(match, ln_experiment_size);
      
      // now add match to match collection
      add_match_to_post_match_collection(match_collection, match);
    }// next match for this spectrum

  }// next spectrum
  
  return TRUE;
}

/**
 * \brief Adds the match to match_collection by copying the pointer.
 * 
 * No new match is allocated.  Match_collection total_matches must not
 * exceed the _MAX_NUMBER_PEPTIDES. 
 * \returns TRUE if successfully adds the match to the
 * match_collection, else FALSE 
 */
BOOLEAN_T add_match_to_match_collection(
  MATCH_COLLECTION_T* match_collection, ///< the match collection to free -out
  MATCH_T* match ///< the match to add -in
  )
{
  if( match_collection == NULL || match == NULL ){
    carp(CARP_FATAL, "Cannot add NULL match to NULL collection.");
  }

  // check if enough space for peptide match
  if(match_collection->match_total >= _MAX_NUMBER_PEPTIDES){
    carp(CARP_FATAL, "Cannot add to match collection; count exceeds limit: %d", 
         _MAX_NUMBER_PEPTIDES);
  }

  // add a new match to array
  match_collection->match[match_collection->match_total] = match;
  increment_match_pointer_count(match);
  
  // increment total rich match count
  ++match_collection->match_total;

  
  return TRUE;
}

/**
 * Adds the match object to match_collection
 * Must not exceed the _MAX_NUMBER_PEPTIDES to be match added
 * Only for post_process (i.e. post search) match_collections.  Keeps
 * track of all peptides in a hash table.
 * \returns TRUE if successfully adds the match to the
 * match_collection, else FALSE 
 */
// this method renamed so that a more general add_match_to_match_collection could be implemented
BOOLEAN_T add_match_to_post_match_collection(
  MATCH_COLLECTION_T* match_collection, ///< the match collection to free -out
  MATCH_T* match ///< the match to add -in
  )
{
  if( match_collection == NULL || match == NULL ){
    carp(CARP_FATAL, "Cannot add NULL match to NULL collection.");
  }
  PEPTIDE_T* peptide = NULL;

  // only for post_process_collections
  if(!match_collection->post_process_collection){
    carp(CARP_ERROR, "Must be a post process match collection to add a match.");
    return FALSE;
  }

  // check if enough space for peptide match
  if(match_collection->match_total >= _MAX_NUMBER_PEPTIDES){
    carp(CARP_ERROR, "Rich match count exceeds max match limit: %d", 
         _MAX_NUMBER_PEPTIDES);
    return FALSE;
  }
  
  // add a new match to array
  match_collection->match[match_collection->match_total] = match;
  increment_match_pointer_count(match);
  
  // increment total rich match count
  ++match_collection->match_total;
  
  // DEBUG, print total peptided scored so far
  if(match_collection->match_total % 1000 == 0){
    carp(CARP_INFO, "parsed PSM: %d", match_collection->match_total);
  }
  
  // match peptide
  peptide = get_match_peptide(match);
  
  // update protein counter, protein_peptide counter
  update_protein_counters(match_collection, peptide);
  
  // update hash table
  char* hash_value = get_peptide_hash_value(peptide); 
  add_hash(match_collection->post_hash, hash_value, NULL); 
  
  return TRUE;
}

/**
 * updates the protein_counter and protein_peptide_counter for 
 * run specific features
 */
void update_protein_counters(
  MATCH_COLLECTION_T* match_collection, ///< working match collection -in
  PEPTIDE_T* peptide  ///< peptide information to update counters -in
  )
{
  PEPTIDE_SRC_ITERATOR_T* src_iterator = NULL;
  PEPTIDE_SRC_T* peptide_src = NULL;
  PROTEIN_T* protein = NULL;
  unsigned int protein_idx = 0;
  int hash_count = 0;
  BOOLEAN_T unique = FALSE;
  
  // only for post_process_collections
  if(!match_collection->post_process_collection){
    carp(CARP_FATAL, 
         "Must be a post process match collection to update protein counter.");
  }
  
  // See if this peptide has been observed before?
  char* hash_value = get_peptide_hash_value(peptide);
  hash_count = get_hash_count(match_collection->post_hash, hash_value);
  free(hash_value);

  if(hash_count < 1){
    // yes this peptide is first time observed
    unique = TRUE;
  }

  // first update protein counter
  src_iterator = new_peptide_src_iterator(peptide);
  
  // iterate overall parent proteins
  while(peptide_src_iterator_has_next(src_iterator)){
    peptide_src = peptide_src_iterator_next(src_iterator);
    protein = get_peptide_src_parent_protein(peptide_src);
    protein_idx = get_protein_protein_idx(protein);
    
    // update the number of PSM this protein matches
    ++match_collection->post_protein_counter[protein_idx];
    
    // number of peptides match this protein
    if(unique){
      ++match_collection->post_protein_peptide_counter[protein_idx];
    }
  }  
  
  free_peptide_src_iterator(src_iterator);
}

/**
 * Fill the match objects score with the given the FLOAT_T array. 
 * The match object order must not have been altered since scoring.
 * The result array size must match the match_total count.
 * Match ranks are also populated to preserve the original order of the
 * match input TRUE for preserve_order.
 *\returns TRUE, if successfully fills the scores into match object, else FALSE.
 */
BOOLEAN_T fill_result_to_match_collection(
  MATCH_COLLECTION_T* match_collection, 
    ///< the match collection to iterate -out
  double* results,  
    ///< The result score array to fill the match objects -in
  SCORER_TYPE_T score_type,  
    ///< The score type of the results to fill (XCORR, Q_VALUE, ...) -in
  BOOLEAN_T preserve_order 
    ///< preserve match order?
  )
{
  int match_idx = 0;
  MATCH_T* match = NULL;
  MATCH_T** match_array = NULL;
  SCORER_TYPE_T score_type_old = match_collection->last_sorted;

  // iterate over match object in collection, set scores
  for(; match_idx < match_collection->match_total; ++match_idx){
    match = match_collection->match[match_idx];
    set_match_score(match, score_type, results[match_idx]);    
  }
  
  // if need to preserve order store a copy of array in original order 
  if(preserve_order){
    match_array = (MATCH_T**)mycalloc(match_collection->match_total, sizeof(MATCH_T*));
    for(match_idx=0; match_idx < match_collection->match_total; ++match_idx){
      match_array[match_idx] = match_collection->match[match_idx];
    }
  }

  // populate the rank of match_collection
  if(!populate_match_rank_match_collection(match_collection, score_type)){
    free_match_collection(match_collection);
    carp(CARP_FATAL, "failed to populate match rank in match_collection");
  }
  
  // restore match order if needed
  if(preserve_order){
    for(match_idx=0; match_idx < match_collection->match_total; ++match_idx){
      match_collection->match[match_idx] = match_array[match_idx];
    }
    match_collection->last_sorted = score_type_old;
    free(match_array);
  }

  match_collection->scored_type[score_type] = TRUE;
  
  return TRUE;
}

/**
 * Process run specific features from all the PSMs
 */
void process_run_specific_features(
  MATCH_COLLECTION_T* match_collection ///< the match collection to free -out
  );

/**
 * \brief Calculate the delta_cn of each match and populate the field.
 * 
 * Delta_cn is the xcorr difference between match[i] and match[i+1]
 * divided by the xcorr of match[0].  This could be generalized to
 * whichever score is the main one.  Sorts by xcorr, if necessary.
 * 
 */
BOOLEAN_T calculate_delta_cn( MATCH_COLLECTION_T* match_collection){

  if( match_collection == NULL ){
    carp(CARP_ERROR, "Cannot calculate deltaCn for NULL match collection");
    return FALSE;
  }

  if( match_collection->scored_type[XCORR] == FALSE ){
    carp(CARP_WARNING, 
      "Delta_cn not calculated because match collection not scored for xcorr");
    return FALSE;
  }

  // sort, if not already
  MATCH_T** matches = match_collection->match;
  int num_matches = match_collection->match_total;
  if( match_collection->last_sorted != XCORR ){
    qsort_match(matches, num_matches, (void*)compare_match_xcorr);
    match_collection->last_sorted = XCORR;
  }

  // get xcorr of first match
  FLOAT_T max_xcorr = get_match_score(matches[0], XCORR);

  // for each match, calculate deltacn
  int match_idx=0;
  for(match_idx=0; match_idx < num_matches; match_idx++){
    FLOAT_T diff = max_xcorr - get_match_score(matches[match_idx], XCORR);
    double delta_cn = diff / max_xcorr;
    if( delta_cn == 0 ){ // I hate -0, this prevents it
      delta_cn = 0.0;
    }
    set_match_delta_cn(matches[match_idx], delta_cn);
  }

  return TRUE;
}


/**********************************
 * match_collection get, set methods
 **********************************/

/**
 * \returns TRUE if the match_collection only contains decoy matches,
 * else (all target or mixed) returns FALSE.
 */
BOOLEAN_T get_match_collection_is_decoy(
  MATCH_COLLECTION_T* match_collection
){
  return match_collection->null_peptide_collection;
}

/**
 *\returns the match_collection protein counter for the protein idx
 */
int get_match_collection_protein_counter(
  MATCH_COLLECTION_T* match_collection, ///< the working match collection -in
  unsigned int protein_idx ///< the protein index to return protein counter -in
  )
{
  // only for post_process_collections
  if(!match_collection->post_process_collection){
    carp(CARP_FATAL, "Must be a post process match collection to get protein counter.");
  }

  // number of PSMs match this protein
  return match_collection->post_protein_counter[protein_idx];
}

/**
 *\returns the match_collection protein peptide counter for the protein idx
 */
int get_match_collection_protein_peptide_counter(
  MATCH_COLLECTION_T* match_collection, ///< the working match collection -in
  unsigned int protein_idx ///< the protein index to return protein peptiide counter -in
  )
{
  // only for post_process_collections
  if(!match_collection->post_process_collection){
    carp(CARP_FATAL, "Must be a post process match collection to get peptide counter.");
  }
  
  // number of peptides match this protein
  return match_collection->post_protein_peptide_counter[protein_idx];
}

/**
 *\returns the match_collection hash value of PSMS for which this is the best scoring peptide
 */
int get_match_collection_hash(
  MATCH_COLLECTION_T* match_collection, ///< the working match collection -in
  PEPTIDE_T* peptide  ///< the peptide to check hash value -in
  )
{
  // only for post_process_collections
  if(!match_collection->post_process_collection){
    carp(CARP_FATAL, "Must be a post process match collection, to get match_collection_hash");
  }
  
  char* hash_value = get_peptide_hash_value(peptide);
  int count = get_hash_count(match_collection->post_hash, hash_value);
  free(hash_value);
  
  return count;
}

/**
 * \brief Get the number of proteins in the database associated with
 * this match collection.
 */
int get_match_collection_num_proteins(
  MATCH_COLLECTION_T* match_collection ///< the match collection of interest -
  ){

  return match_collection->post_protein_counter_size;
}


/******************************
 * match_collection_iterator
 ******************************/
     
/**
 * \brief Finds the next match_collection in directory and prepares
 * the iterator to hand it off when 'next' called.
 *
 * When no more match_collections (i.e. psm files) are available, set
 * match_collection_iterator->is_another_collection to FALSE
 * \returns void
 */
void setup_match_collection_iterator(
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator 
    ///< the match_collection_iterator to set up -in/out
  )
{
  // are there any more match_collections to return?
  if(match_collection_iterator->collection_idx 
      < match_collection_iterator->number_collections){

    // then go parse the match_collection
    match_collection_iterator->match_collection = 
      new_match_collection_psm_output(match_collection_iterator, 
         (SET_TYPE_T)match_collection_iterator->collection_idx);

    // we have another match_collection to return
    match_collection_iterator->is_another_collection = TRUE;
    
    // let's move on to the next one next time
    ++match_collection_iterator->collection_idx;

    // reset directory
    rewinddir(match_collection_iterator->working_directory);
  }
  else{
    // we're done, no more match_collections to return
    match_collection_iterator->is_another_collection = FALSE;
  }
}

/**
 * Create a match_collection iterator from a directory of serialized files
 * Only hadles up to one target and three decoy sets per folder
 *\returns match_collection iterator instantiated from a result folder
 */
MATCH_COLLECTION_ITERATOR_T* new_match_collection_iterator(
  char* output_file_directory, 
    ///< the directory path where the PSM output files are located -in
  char* fasta_file, 
    ///< The name of the fasta file for peptides for match_collections. -in
  int* decoy_count
  )
{
  carp(CARP_DEBUG, 
       "Creating match collection iterator for dir %s and protein input %s",
       output_file_directory, fasta_file);

  // allocate match_collection
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator =
    (MATCH_COLLECTION_ITERATOR_T*)
      mycalloc(1, sizeof(MATCH_COLLECTION_ITERATOR_T));

  DIR* working_directory = NULL;
  struct dirent* directory_entry = NULL;
  DATABASE_T* database = NULL;
  BOOLEAN_T use_index = is_directory(fasta_file);

  /*
    BF: I think that this step is to count how many decoys there are
    per target file.  This is prone to errors as all it really does is
    check for the presence of a file with *decoy_1*, and one with
    *decoy_2* and *decoy_3*.  In fact, the three files could be from
    different targets.  Nothing was being done with the check for a
    target file.  There must be a better way to do this.
   */


  // do we have these files in the directory
  BOOLEAN_T boolean_result = FALSE;
  BOOLEAN_T decoy_1 = FALSE;
  BOOLEAN_T decoy_2 = FALSE;
  BOOLEAN_T decoy_3 = FALSE;

  // open PSM file directory
  working_directory = opendir(output_file_directory);
  
  if(working_directory == NULL){
    carp(CARP_FATAL, "Failed to open PSM file directory: %s", 
        output_file_directory);
  }
  
  // determine how many decoy sets we have
  while((directory_entry = readdir(working_directory))){
    
    if(suffix_compare(directory_entry->d_name, "decoy-1.csm")) {
      carp(CARP_DEBUG, "Found decoy file %s", directory_entry->d_name);
      decoy_1 = TRUE;
    }
    else if(suffix_compare(directory_entry->d_name, "decoy.csm")) {
      decoy_1 = TRUE;
    }
    else if(suffix_compare(directory_entry->d_name, "decoy-2.csm")) {
      decoy_2 = TRUE;
    }
    else if(suffix_compare(directory_entry->d_name, "decoy-3.csm")) {
      decoy_3 = TRUE;
    }    
    else if(suffix_compare(directory_entry->d_name, ".csm")){
      carp(CARP_DEBUG, "Found target file %s", directory_entry->d_name);
      boolean_result = TRUE;
    }
    if (boolean_result && decoy_1 && decoy_2 && decoy_3) {
      break; // We've found all the files we can use.
    }
  }
  
  // set total_sets count
  int total_sets = 0;

  if(decoy_3){
    total_sets = 4; // 3 decoys + 1 target
    *decoy_count = 3;
  }
  else if(decoy_2){
    total_sets = 3; // 2 decoys + 1 target
    *decoy_count = 2;
  }
  else if(decoy_1){
    total_sets = 2; // 1 decoys + 1 target
    *decoy_count = 1;
  }
  else{
    total_sets = 1;
    *decoy_count = 0;
    carp(CARP_INFO, "No decoy sets exist in directory: %s", 
        output_file_directory);
  }
  if(!boolean_result){
    carp(CARP_FATAL, "No PSM files found in directory '%s'", 
         output_file_directory);
  }

  // get binary fasta file name with path to crux directory 
  //  char* binary_fasta = get_binary_fasta_name_in_crux_dir(fasta_file);
  char* binary_fasta  = NULL;
  if (use_index == TRUE){ 
    binary_fasta = get_index_binary_fasta_name(fasta_file);
  } else {
    binary_fasta = get_binary_fasta_name(fasta_file);
    carp(CARP_DEBUG, "Looking for binary fasta %s", binary_fasta);
    if (access(binary_fasta, F_OK)){
      carp(CARP_DEBUG, "Could not find binary fasta %s", binary_fasta);
      if (!create_binary_fasta_here(fasta_file, binary_fasta)){
       carp(CARP_FATAL, "Could not create binary fasta file %s", binary_fasta);
      };
    }
  }
  
  // check if input file exist
  if(access(binary_fasta, F_OK)){
    free(binary_fasta);
    carp(CARP_FATAL, "The file \"%s\" does not exist (or is not readable, "
        "or is empty) for crux index.", binary_fasta);
  }
  
  carp(CARP_DEBUG, "Creating a new database");
  // now create a database, 
  // using fasta file either binary_file(index) or fastafile
  database = new_database(binary_fasta, TRUE);
  
  // check if already parsed
  if(!get_database_is_parsed(database)){
    carp(CARP_DETAILED_DEBUG,"Parsing database");
    if(!parse_database(database)){
      carp(CARP_FATAL, "Failed to parse database, cannot create new index");
    }
  }
  
  free(binary_fasta);

  // reset directory
  rewinddir(working_directory);
  
  // set match_collection_iterator fields
  match_collection_iterator->working_directory = working_directory;
  match_collection_iterator->database = database;  
  match_collection_iterator->number_collections = total_sets;
  match_collection_iterator->directory_name = 
    my_copy_string(output_file_directory);
  match_collection_iterator->is_another_collection = FALSE;

  // setup the match collection iterator for iteration
  // here it will go parse files to construct match collections
  setup_match_collection_iterator(match_collection_iterator);

  // clean up strings
  //free(file_path_array);
  //free(filename);
  //free(decoy_prefix);

  return match_collection_iterator;
}

/**
 *\returns TRUE, if there's another match_collection to return, else return FALSE
 */
BOOLEAN_T match_collection_iterator_has_next(
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator ///< the working match_collection_iterator -in
  )
{
  // Do we have another match_collection to return
  return match_collection_iterator->is_another_collection;
}

/**
 * free match_collection_iterator
 */
void free_match_collection_iterator(
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator ///< the working match_collection_iterator -in
  )
{
  // free unclaimed match_collection
  if(match_collection_iterator->match_collection != NULL){
    free_match_collection(match_collection_iterator->match_collection);
  }
  
  // if no index, remove the temp binary fasta file
  char* fasta_file = get_string_parameter("protein input");
  if( is_directory(fasta_file) == FALSE ){
    char* binary_fasta = get_binary_fasta_name(fasta_file);
    carp(CARP_DEBUG, "Protein source %s is not an index.  "
         "Removing temp binary fasta %s", fasta_file, binary_fasta);
    remove(binary_fasta);
  }

  // free up all match_collection_iterator 
  free(match_collection_iterator->directory_name);
  free_database(match_collection_iterator->database);
  closedir(match_collection_iterator->working_directory); 
  free(match_collection_iterator);
}

/**
 * \brief Fetches the next match collection object and prepares for
 * the next iteration 
 *\returns The next match collection object
 */
MATCH_COLLECTION_T* match_collection_iterator_next(
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator 
    ///< the working match_collection_iterator -in
  )
{
  MATCH_COLLECTION_T* match_collection = NULL;
  
  if(match_collection_iterator->is_another_collection){
    match_collection = match_collection_iterator->match_collection;
    match_collection_iterator->match_collection = NULL;
    setup_match_collection_iterator(match_collection_iterator);
    return match_collection;
  }
  else{
    carp(CARP_ERROR, "No match_collection to return");
    return NULL;
  }
}

/**
 *\returns the total number of match_collections to return
 */
int get_match_collection_iterator_number_collections(
  MATCH_COLLECTION_ITERATOR_T* match_collection_iterator ///< the working match_collection_iterator -in
  )
{
  return match_collection_iterator->number_collections;
}

/**
 * \brief Get the name of the directory the match_collection_iterator
 * is working in.
 * \returns A heap allocated string (char*) of the directory name.
 */
char* get_match_collection_iterator_directory_name(
  MATCH_COLLECTION_ITERATOR_T* iterator ///< the match_collection_iterator -in
  ){

  char* dir_name = my_copy_string(iterator->directory_name);

  return dir_name;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
