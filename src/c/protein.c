/*****************************************************************************
 * \file protein.c
 * $Revision: 1.15 $
 * \brief: Object for representing a single protein.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "alphabet.h"
#include "objects.h"
#include "peptide.h"
#include "protein.h"
#include "protein_peptide_association.h"

/**
 * Constants
 */
#define PROTEIN_ID_LENGTH 100
#define PROTEIN_SEQUENCE_LENGTH 10000
#define PROTEIN_ANNOTATION_LENGTH 100
#define LONGEST_LINE PROTEIN_ID_LENGTH + PROTEIN_ID_LENGTH
#define FASTA_LINE 50

/**
 * \struct protein 
 * \brief A protein sequence.
 */
struct protein{
  char*         id; ///< The protein sequence id.
  char*   sequence; ///< The protein sequence.
  int       length; ///< The length of the protein sequence.
  char* annotation; ///< Optional protein annotation. 
};    


/**
 * \struct protein_peptide_iterator
 * \brief Object to iterate over the peptides within a protein in an
 * unspecified order. The peptides should satisfy the constraints specified
 * in the peptide_constraint object.
 * 
 */
struct protein_peptide_iterator {
  PROTEIN_T* protein; ///< The protein whose peptides to iterate over. 
  unsigned short int cur_start; ///< Start in protein of the current peptide.
  unsigned char cur_length; ///< The length of the current peptide.
  unsigned int peptide_idx; ///< The index of the current peptide.
  PEPTIDE_CONSTRAINT_T* peptide_constraint; ///< The type of peptide to iterate over.
  float** mass_matrix; ///< stores all the peptide's mass
  BOOLEAN_T has_next; ///< is there a next? 
  int num_mis_cleavage; ///< The maximum mis cleavage of the peptide
};

//def bellow
static BOOLEAN_T read_title_line
  (FILE* fasta_file,
   char* name,
   char* description);

//def bellow
static BOOLEAN_T read_raw_sequence
  (FILE* fasta_file,   // Input Fasta file.
   char* name,         // Sequence ID (used in error messages).
   int   max_chars,    // Maximum number of allowed characters.
   char* raw_sequence, // Pre-allocated sequence.
   int* sequence_length // the sequence length -chris added
   );


/**
 * \returns An (empty) protein object.
 */
PROTEIN_T* allocate_protein(void){
  PROTEIN_T* protein = (PROTEIN_T*)mycalloc(1, sizeof(PROTEIN_T));
  return protein;
}

/**
 * \returns A new protein object.
 */
PROTEIN_T* new_protein(
  char*         id, ///< The protein sequence id.
  char*   sequence, ///< The protein sequence.
  int       length, ///< The length of the protein sequence.
  char* annotation  ///< Optional protein annotation. 
  )
{
  PROTEIN_T* protein = allocate_protein();
  set_protein_id(protein, id);
  set_protein_sequence(protein, sequence);
  set_protein_length(protein, length);
  set_protein_annotation(protein, annotation);
  return protein;
}         

/**
 * Frees an allocated protein object.
 */
void free_protein(
  PROTEIN_T* protein ///< object to free -in
  )
{
  free(protein->id);
  free(protein->sequence);
  free(protein->annotation);
  free(protein);
}

/**
 * Prints a protein object to file.
 */
void print_protein(
  PROTEIN_T* protein, ///< protein to print -in
  FILE* file ///< output stream -out
  )
{
  int   sequence_index;
  int   sequence_length = get_protein_length(protein);
  char* sequence = get_protein_sequence(protein);
  char* id = get_protein_id(protein);
  char* annotation = get_protein_annotation(protein);
  
  fprintf(file, ">%s %s\n", id, annotation);

  sequence_index = 0;
  while (sequence_length - sequence_index > FASTA_LINE) {
    fprintf(file, "%.*s\n", FASTA_LINE, &(sequence[sequence_index]));
    sequence_index += FASTA_LINE;
  }
  fprintf(file, "%s\n\n", &(sequence[sequence_index]));

  free(sequence);
  free(id);
  free(annotation);
}

/**
 * Copies protein object src to dest.
 * dest must be a heap allocated object 
 */
void copy_protein(
  PROTEIN_T* src,///< protein to copy -in
  PROTEIN_T* dest ///< protein to copy to -out
  )
{
  char* id = get_protein_id(src);
  char* sequence = get_protein_sequence(src);
  char* annotation = get_protein_annotation(src);
  
  set_protein_id(dest, id);
  set_protein_sequence(dest, sequence);
  set_protein_length(dest, get_protein_length(src));
  set_protein_annotation(dest, annotation);

  free(id);
  free(sequence);
  free(annotation);
}


//FIXME ID line and annotation might need to be fixed
VERBOSE_T verbosity = NORMAL_VERBOSE;
/**
 * Parses a protein from an open (FASTA) file.
 * \returns TRUE if success. FALSE is failure.
 * protein must be a heap allocated
 */
BOOLEAN_T parse_protein_fasta_file(
  PROTEIN_T* protein, ///< protein object to fill in -out
  FILE* file ///< fasta file -in
  )
{
  static char name[LONGEST_LINE];     // Just the sequence ID.
  static char desc[LONGEST_LINE];     // Just the comment field.
  static char buffer[PROTEIN_SEQUENCE_LENGTH];        // The sequence, as it's read in.
  static int sequence_length; //the sequence length

  // Read the title line.
  if (!read_title_line(file, name, desc)) {
    return(FALSE);
  }
  
  //need this line to initialize alphabet to set for protein instead of DNA
  set_alphabet(verbosity, "ACDEFGHIKLMNPQRSTVWY"); 
  buffer[0] = '\0';

  // Read the sequence.
  if (!read_raw_sequence(file, name, PROTEIN_SEQUENCE_LENGTH, buffer, &sequence_length)) {
    die("Sequence %s is too long.\n", name);
  }
    
  //update the protein object.
  set_protein_length(protein, sequence_length);
  set_protein_id(protein, name);
  set_protein_sequence(protein, buffer);
  set_protein_annotation(protein, desc);

  return(TRUE);

}

/**************************************************/

/**
 * FASTA file parsing code
 * AUTHOR: William Stafford Noble
 * modified by Chris Park
 */

/**
 * Find the beginning of the next sequence, and read the sequence ID
 * and the comment.
 */
static BOOLEAN_T read_title_line
  (FILE* fasta_file,
   char* name,
   char* description)
{
  static char id_line[LONGEST_LINE];  // Line containing the ID and comment.
  int a_char;                         // The most recently read character.

  // Read until the first occurrence of ">".
  while ((a_char = getc(fasta_file)) != '>') {
    // If we hit the end of the file, return FALSE.
    if (a_char == EOF) {
      return(FALSE);
    }
  }

  // Read the ID and comment line.
  if (fgets(id_line, LONGEST_LINE-1, fasta_file) == NULL) {
    die("Error reading Fasta file.\n");
  }

  // Remove EOL.
  id_line[strlen(id_line) - 1] = '\0';

  // Extract the ID from the beginning of the line.
  if (sscanf(id_line, "%s", name) != 1) {
    die("Error reading sequence ID.\n%s\n", id_line);
  }

  // Store the rest of the line as the comment.
  strcpy(description, &(id_line[strlen(name)+1]));

  return(TRUE);
}


/****************************************************************************
 * Read raw sequence until a '>' is encountered or too many letters
 * are read.  The new sequence is appended to the end of the given
 * sequence.
 *
 * Return: Was the sequence read completely?
 ****************************************************************************/
static BOOLEAN_T read_raw_sequence
  (FILE* fasta_file,   // Input Fasta file.
   char* name,         // Sequence ID (used in error messages).
   int   max_chars,    // Maximum number of allowed characters.
   char* raw_sequence, // Pre-allocated sequence.
   int* sequence_length // the sequence length -chris added
   )
{
  // char a_char;
  // tlb; change a_char to integer so it will compile on SGI
  int a_char;
  int i_seq;
  BOOLEAN_T return_value = TRUE;

  // Start at the end of the given sequence.
  i_seq = strlen(raw_sequence);
  assert((int)strlen(raw_sequence) < max_chars);

  // Read character by character.
  while ((a_char = getc(fasta_file)) != EOF) {

    // Check for the beginning of the next sequence.
    if (a_char == '>') {
      // Put the ">" back onto the stream for the next call to find.
      ungetc(a_char, fasta_file);
      break;
    }

    // Skip non-alphabetic characters.
    if (!isalpha((int)a_char)) {
      if ((a_char != ' ') && (a_char != '\t') && (a_char != '\n') && (a_char != '\r')) {
	fprintf(stderr, "Warning: Skipping character %c in sequence %s.\n",
		a_char, name);
      }

    } else {

      // Convert invalid characters to X.
      a_char = toupper((int)a_char);
      if (!char_in_string(get_alphabet(TRUE), a_char)) {
	fprintf(stderr, "Warning: Converting illegal character %c to X ",
		a_char);
	fprintf(stderr, "in sequence %s.\n", name);
	a_char = 'X';
      }
      raw_sequence[i_seq] = a_char;
      i_seq++;
    }
    if (i_seq >= max_chars) {
      return_value = FALSE;
      break;
    }
  }
  raw_sequence[i_seq] = '\0';
  *sequence_length = i_seq; // chris added

  return(return_value);
}

/**
 * end of FASTA parsing
 * Thanks Bill!
 */


/** 
 * Access routines of the form get_<object>_<field> and set_<object>_<field>. 
 */

/**
 * Additional get and set methods
 */

/**
 *\returns the id of the protein
 * returns a heap allocated new copy of the id
 * user must free the return id
 */
char* get_protein_id(
  PROTEIN_T* protein ///< the query protein -in 
  )
{
  int id_length = strlen(protein->id) +1; //+\0
  char* copy_id = 
    (char *)mymalloc(sizeof(char)*id_length);
  return strncpy(copy_id, protein->id, id_length); 
}

/**
 * sets the id of the protein
 */
void set_protein_id(
  PROTEIN_T* protein, ///< the protein to set it's fields -out
  char* id ///< the sequence to add -in
  )
{
  free(protein->id);
  int id_length = strlen(id) +1; //+\0
  char* copy_id = 
    (char *)mymalloc(sizeof(char)*id_length);
  protein->id =
    strncpy(copy_id, id, id_length);  
}

/**
 *\returns the sequence of the protein
 * returns a heap allocated new copy of the sequence
 * user must free the return sequence 
 */
char* get_protein_sequence(
  PROTEIN_T* protein ///< the query protein -in 
  )
{
  int sequence_length = strlen(protein->sequence) +1; //+\0
  char * copy_sequence = 
    (char *)mymalloc(sizeof(char)*sequence_length);
  return strncpy(copy_sequence, protein->sequence, sequence_length);  
}

/**
 * sets the sequence of the protein
 */
void set_protein_sequence(
  PROTEIN_T* protein, ///< the protein to set it's fields -out
  char* sequence ///< the sequence to add -in
  )
{
  free(protein->sequence);
  int sequence_length = strlen(sequence) +1; //+\0
  char * copy_sequence = 
    (char *)mymalloc(sizeof(char)*sequence_length);

  protein->sequence =
    strncpy(copy_sequence, sequence, sequence_length);  
}

/**
 *\returns the length of the protein
 */
int get_protein_length(
  PROTEIN_T* protein ///< the query protein -in 
  )
{
  return protein->length;
}

/**
 * sets the id of the protein
 */
void set_protein_length(
  PROTEIN_T* protein, ///< the protein to set it's fields -out
  int length ///< the length to add -in
  )
{
  protein->length = length;
}

/**
 *\returns the annotation of the protein
 * returns a heap allocated new copy of the annotation
 * user must free the return annotation
 */
char* get_protein_annotation(
  PROTEIN_T* protein ///< the query protein -in 
  )
{
  int annotation_length = strlen(protein->annotation) +1; //+\0
  char * copy_annotation = 
    (char *)mymalloc(sizeof(char)*annotation_length);
  return strncpy(copy_annotation, protein->annotation, annotation_length);  
}

/**
 * sets the annotation of the protein
 */
void set_protein_annotation(
  PROTEIN_T* protein, ///< the protein to set it's fields -out
  char* annotation ///< the sequence to add -in
  )
{
  free(protein->annotation);
  int annotation_length = strlen(annotation) +1; //+\0
  char * copy_annotation = 
    (char *)mymalloc(sizeof(char)*annotation_length);
  protein->annotation =
    strncpy(copy_annotation, annotation, annotation_length);  
}

/**
 * Iterator
 * iterates over the peptides given a partent protein and constraints
 */

/**
 * examines the peptide with context of it's parent protein to determine it's type
 * \returns the peptide type
 */
PEPTIDE_TYPE_T examine_peptide_type(
  char* sequence, ///< the parent protein -in
  int start_idx, ///< the start index of peptide, 1 is the first residue -in 
  int end_idx ///< the end index of peptide -in
  )
{
  int current_idx = start_idx-1;
  BOOLEAN_T start = TRUE;
  BOOLEAN_T end =TRUE;

  //check start position must be cleaved at K or R residue
  if(current_idx != 0){
    if(sequence[current_idx-1] != 'K' && sequence[current_idx-1] != 'R'){
      start = FALSE;
    }
    else if(sequence[current_idx] == 'P'){
      start = FALSE;
    }         
  }

  //check if last residue is K or R not followed by P
  if(end_idx < strlen(sequence)){
    if(sequence[end_idx-1] != 'K' && sequence[end_idx-1] != 'R'){
      end = FALSE;
    }
    else if(sequence[end_idx] == 'P'){
      end = FALSE;
    }
  }
  
  if(start && end){
    return TRYPTIC;
  }
  else if(start || end){
    return PARTIALLY_TRYPTIC;
  }
  else{
    return NOT_TRYPTIC;
  }
}

//FIXME only examines if there is a mis-cleavage or not
// eventually would like to implement so that it will return the total number of mis-cleavage
/**
 * examines the peptide if it contains miscleavage sites within it's sequence
 * \returns 0 if no miscleavage sites, 1 if there exist at least 1 mis cleavage sites
 */
int examine_peptide_cleavage(
  char* sequence, ///< the parent protein -in
  int start_idx, ///< the start index of peptide, 1 is the first residue -in 
  int end_idx ///< the end index of peptide -in
  )
{
  int current_idx = start_idx-1;

  //check for instances of K, R in the sequence excluding the last residue
  for(; current_idx < end_idx-1; ++current_idx){
    if(sequence[current_idx] == 'K' || sequence[current_idx] == 'R'){
      if(sequence[current_idx+1] != 'P'){
        return 1;
      }
    }
  }
  return 0;

}

/**
 * recursively calls itself to find the next peptide that fits the constraints
 * \returns TRUE if there is a next peptide. FALSE if not.
 */
BOOLEAN_T iterator_state_help(
  PROTEIN_PEPTIDE_ITERATOR_T* iterator, 
  int max_length,  ///< constraints: max length -in
  int min_length, ///< constraints: min length -in
  float max_mass, ///< constraints: max mass -in
  float min_mass, ///< constraints: min mass -in
  PEPTIDE_TYPE_T peptide_type  ///< constraints: peptide type -in
  )
{
  //check if out of mass_max idex size
  if(iterator->cur_length > max_length ||
     iterator->cur_length > iterator->protein->length){
    return FALSE;
  }
  
  //check if less than min length
  if(iterator->cur_length < min_length){
    ++iterator->cur_length;
    return iterator_state_help(iterator, max_length, min_length, max_mass, min_mass, peptide_type);
  }
  
  //reached end of length column, check next length
  if(iterator->cur_start > iterator->protein->length){
    ++iterator->cur_length;
    iterator->cur_start = 1;
    return iterator_state_help(iterator, max_length, min_length, max_mass, min_mass, peptide_type);
  }

  //is mass with in range
  if(iterator->mass_matrix[iterator->cur_length-1][iterator->cur_start-1] < min_mass ||
        iterator->mass_matrix[iterator->cur_length-1][iterator->cur_start-1] > max_mass){
    if(iterator->mass_matrix[iterator->cur_length-1][iterator->cur_start-1] == 0){
      ++iterator->cur_length;
      iterator->cur_start = 1;
    }
    else{
      ++iterator->cur_start;
    }
    return iterator_state_help(iterator, max_length, min_length, max_mass, min_mass, peptide_type);
  }
  
  //examin tryptic type and cleavage
  if(peptide_type != ANY_TRYPTIC){
    if((examine_peptide_type(iterator->protein->sequence, 
                             iterator->cur_start, 
                             iterator->cur_length + iterator->cur_start -1) != peptide_type))
      {
        ++iterator->cur_start;
        return iterator_state_help(iterator, max_length, min_length, max_mass, min_mass, peptide_type);
      }
  }

  //examine cleavage
  if(iterator->num_mis_cleavage == 0){
    if(examine_peptide_cleavage(iterator->protein->sequence, 
                                iterator->cur_start, 
                                iterator->cur_length + iterator->cur_start -1) != 0)
      {
        ++iterator->cur_start;
        return iterator_state_help(iterator, max_length, min_length, max_mass, min_mass, peptide_type);
      }
  }
  
  
  return TRUE;
}


/**
 * sets the iterator to the next peptide that fits the constraints
 * \returns TRUE if there is a next peptide. FALSE if not.
 */
BOOLEAN_T set_iterator_state(
  PROTEIN_PEPTIDE_ITERATOR_T* iterator  ///< set iterator to next peptide -in
  )
{
  int max_length = get_peptide_constraint_max_length(iterator->peptide_constraint);
  int min_length = get_peptide_constraint_min_length(iterator->peptide_constraint);
  float max_mass = get_peptide_constraint_max_mass(iterator->peptide_constraint);
  float min_mass = get_peptide_constraint_min_mass(iterator->peptide_constraint);
  PEPTIDE_TYPE_T peptide_type = get_peptide_constraint_peptide_type(iterator->peptide_constraint);
  
  return iterator_state_help(iterator, max_length, min_length, max_mass, min_mass, peptide_type);
}


//start_size: total sequence size 
//length_size: max_length from constraint;
//float** mass_matrix = float[length_size][start_size];
/**
 * Dynamically sets the mass of the mass_matrix
 * The mass matrix contains every peptide bellow max length
 * must pass in a heap allocated matrix
 */
void set_mass_matrix(
  float** mass_matrix,  ///< the mass matrix -out
  int start_size,  ///< the y axis size -in
  int length_size, ///< the x axis size -in
  PEPTIDE_CONSTRAINT_T* peptide_constraint,  ///< the peptide constraints -in
  PROTEIN_T* protein  ///< the parent protein -in
  )
{
  int start_index = 0;
  int length_index = 1;

  //initialize mass matrix
  for(; start_index < start_size; ++start_index){
    mass_matrix[0][start_index] = get_mass_amino_acid_average(protein->sequence[start_index]);
  }
  start_index = 0;
  
  //fill in the mass matrix
  for(; start_index < start_size; ++start_index){
    for(length_index = 1; length_index < length_size; ++length_index){
      if(start_index + length_index < protein->length){
        mass_matrix[length_index][start_index] = 
          mass_matrix[length_index - 1][start_index] + mass_matrix[0][start_index + length_index]; 
      }
    }
  }
}

/**
 * Instantiates a new peptide_iterator from a peptide.
 * \returns a PROTEIN_PEPTIDE_ITERATOR_T object.
 */
PROTEIN_PEPTIDE_ITERATOR_T* new_protein_peptide_iterator(
  PROTEIN_T* protein, ///< the protein's peptide to iterate -in
  PEPTIDE_CONSTRAINT_T* peptide_constraint ///< the peptide constraints -in
  )
{
  int matrix_index = 0;
  int max_length = get_peptide_constraint_max_length(peptide_constraint);
  
  PROTEIN_PEPTIDE_ITERATOR_T* iterator = 
    (PROTEIN_PEPTIDE_ITERATOR_T*)mycalloc(1, sizeof(PROTEIN_PEPTIDE_ITERATOR_T));

  //create mass_matrix
  iterator->mass_matrix = (float**)mycalloc(max_length, sizeof(float*));
  for (; matrix_index < max_length ; ++matrix_index){
    iterator->mass_matrix[matrix_index] = (float*)mycalloc(protein->length, sizeof(float));
  }  
  set_mass_matrix(iterator->mass_matrix, protein->length, max_length, peptide_constraint, protein);
  
  //initialize iterator
  iterator->protein = protein;
  iterator->peptide_idx = 0;
  iterator->peptide_constraint = peptide_constraint;
  iterator->cur_start = 1; // must cur_start-1 for access mass_matrix
  iterator->cur_length = 1;  // must cur_length-1 for access mass_matrix
  iterator->has_next = set_iterator_state(iterator);
  iterator->num_mis_cleavage = get_peptide_constraint_num_mis_cleavage(iterator->peptide_constraint);
  return iterator;
}


/**
 * free the heap allocated mass_matrix
 */
void free_mass_matrix(
  float** mass_matrix,  ///< the mass matrix to free -in
  int length_size ///< the x axis size -in
  )
{
  int matrix_idx = 0;
  for (; matrix_idx  < length_size; ++matrix_idx){
    free(mass_matrix[matrix_idx]);
  }

  free(mass_matrix);
}

/**
 * Frees an allocated peptide_iterator object.
 */
void free_protein_peptide_iterator(
  PROTEIN_PEPTIDE_ITERATOR_T* protein_peptide_iterator ///< the iterator to free -in
  )
{
  free_mass_matrix(protein_peptide_iterator->mass_matrix, 
                   get_peptide_constraint_max_length(protein_peptide_iterator->peptide_constraint));
  free(protein_peptide_iterator);
}

/**
 * The basic iterator functions.
 * \returns TRUE if there are additional peptides to iterate over, FALSE if not.
 */
BOOLEAN_T protein_peptide_iterator_has_next(
  PROTEIN_PEPTIDE_ITERATOR_T* protein_peptide_iterator ///< the iterator of interest -in
  )
{
  return protein_peptide_iterator->has_next;
}


/**
 * \returns The next peptide in the protein, in an unspecified order
 * the Peptide is new heap allocated object, user must free it
 */
PEPTIDE_T* protein_peptide_iterator_next(
  PROTEIN_PEPTIDE_ITERATOR_T* protein_peptide_iterator
  )
{
  PEPTIDE_TYPE_T peptide_type;

  if(!protein_peptide_iterator->has_next){
    free_protein_peptide_iterator(protein_peptide_iterator);
    fprintf(stderr, "ERROR: no more peptides\n");
    exit(1);
  }
  
  //copy peptide sequence
  char peptide_sequence[protein_peptide_iterator->cur_length + 1];
  
  strncpy(peptide_sequence, 
          &protein_peptide_iterator->protein->sequence[protein_peptide_iterator->cur_start-1],
          protein_peptide_iterator->cur_length);
  peptide_sequence[protein_peptide_iterator->cur_length] = '\0';
  
  //set peptide type
  if(get_peptide_constraint_peptide_type(protein_peptide_iterator->peptide_constraint) != ANY_TRYPTIC){
    peptide_type = get_peptide_constraint_peptide_type(protein_peptide_iterator->peptide_constraint);
  }

  //when constraints ANY_TRYPTIC, need to examine peptide
  //possible to skip this step and leave it as ANY_TRYPTIC
  else{
      peptide_type = 
        examine_peptide_type(protein_peptide_iterator->protein->sequence,
                             protein_peptide_iterator->cur_start,
                             protein_peptide_iterator->cur_start + protein_peptide_iterator->cur_length -1);
  }
 
  //create new peptide
  PEPTIDE_T* peptide = 
    new_peptide
    (peptide_sequence, 
     protein_peptide_iterator->cur_length, 
     protein_peptide_iterator->mass_matrix[protein_peptide_iterator->cur_length-1][protein_peptide_iterator->cur_start-1],
     protein_peptide_iterator->protein,
     protein_peptide_iterator->cur_start,
     peptide_type);
  
  //FIXME Not sure what the use delete this field if needed
  ++protein_peptide_iterator->peptide_idx;

  //update poisiton of iterator
  ++protein_peptide_iterator->cur_start;
  protein_peptide_iterator->has_next = set_iterator_state(protein_peptide_iterator);
 
  return peptide;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */

