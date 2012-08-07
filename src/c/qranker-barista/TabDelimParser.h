#ifndef TABDELIMPARSER_H
#define TABDELIMPARSER_H

#include <sys/types.h>
#ifndef WIN32
#include <dirent.h>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <assert.h>
#include <vector>
#include <string>
#include <math.h>
#include <map>
#include <set>
#include <cstring>
#include <stdlib.h>
#include "objects.h"
#include "SpecFeatures.h"
#include "BipartiteGraph.h"
using namespace std;

class TabDelimParser{
 public:

  TabDelimParser();
  ~TabDelimParser();
  void clear();
  inline void set_output_dir(string &dir){out_dir = dir;}
  inline string& get_output_dir(){return out_dir;}
  int run(vector<string> &filenames);

  static void get_tokens(string &line, vector<string>&tokens, string &delim);
  void first_pass(ifstream &fin);
  void second_pass(ifstream &fin, int label);
  void allocate_feature_space();
  void extract_psm_features(vector<string> & tokens, double *x);
  void save_data_in_binary(string out_dir);
  void clean_up(string dir);
  /*******************************************************/
  int run_on_xlink(vector<string> &filenames, string &ms2filename, double xlink_mass);
  int run_on_xlink(vector<string> &filenames);
  void first_pass_xlink(ifstream &fin);
  void second_pass_xlink(ifstream &fin, int label);
  void allocate_feature_space_xlink();
  void extract_xlink_features(int psmind, vector<string> & tokens, double *x);
  void save_data_in_binary_xlink(string out_dir);
  void clean_up_xlink(string dir);
  int get_peptide_length_sum(string& sequence);
  static XLINK_PRODUCT_T get_peptide_type(string& sequence);
  void set_use_quadratic_features(int use);
  
  bool isMissedTryptic(std::string& sequence, int idx);
  int cntMissedCleavagesLinear(int psmind);
  int cntMissedCleavagesSelfLoop(int psmind);
  int cntMissedCleavagesCrossLink(int psmind);
  int cntMissedCleavagesDeadLink(int psmind);
  int cntMissedCleavages(int psmind);

  void get_xcorr_short_long(int psmind, double& short_xcorr, double& long_xcorr);

  int get_peptide_length_short(int psmind);
  int get_peptide_length_long(int psmind);
  int get_peptide_length_sum(int psmind);

//  int get_num_enzymatic_ends(int psmind);
  bool get_nterm1(int psmind);
  bool get_cterm1(int psmind);

  bool get_nterm2(int psmind);
  bool get_cterm2(int psmind);

 protected:

  //spec features generator
  SpecFeaturesGenerator sfg;

  //auxiliary variables
  int num_mixed_labels;
  
  //psm info
  int* psmind_to_scan;
  int* psmind_to_charge;
  int* psmind_to_label;
  int* psmind_to_num_pep;
  int* psmind_to_ofst;
  int* psmind_to_pepind;
  double *psmind_to_neutral_mass;
  double *psmind_to_peptide_mass;

  //peptide info
  map<string,int> pep_to_ind;
  map<int,string> ind_to_pep;
  
  //summary of the dataset
  int num_features;
  int num_spec_features;
  int num_total_features;
  int use_quadratic_features;
  int num_psm;
  int num_pos_psm;
  int num_neg_psm;
  int num_pep;
  int num_pep_in_all_psms;
  int curr_ofst;

  int psmind;

  //psm feature vector
  double *x;
  //spec feature vector
  double *xs;

  //writing out data
  string out_dir;
  ofstream f_psm;

  //final hits per spectrum
  int fhps;
  //decoy prefix
  string decoy_prefix;
  //enzyme
  // enzyme e;
  //max peptide length to be considered
  int max_len;
  //min peptide length to be considered
  int min_len;

  
  int num_base_features;
  int num_charge_features;

  std::set<int> charges;
  std::vector<int> charge_vec;

  bool have_spectra_;

  /************xlink-specific*************/
  int num_xlink_features;
  map<int,string> psmind_to_peptide1;
  map<int,string> psmind_to_peptide2;
  map<int,string> psmind_to_loc;
  map<int,int> psmind_to_loc1;
  map<int,int> psmind_to_loc2;
  map<int,string> psmind_to_protein1;
  map<int,string> psmind_to_protein2;
  map<int,string> psmind_to_flankingaas;

  
  void calc_xlink_locations(int psmind, int& loc1, int& loc2);
  void get_xlink_locations(int psmind, int& loc1, int& loc2);
  XLINK_PRODUCT_T get_peptide_type(int psmind);

  void enzTerm(
    int psmind,
    bool& nterm1,
    bool& cterm1,
    bool& nterm2,
    bool& cterm2
    );

  bool enzNTerm(
    string& sequence, 
    std::vector<string>& flankingaas
    );

  bool enzCTerm(
    string& sequence, 
    std::vector<string>& flankingaas
    );

  int get_num_enzymatic_ends(
    int psmind
  );


  void get_flanking_aas(
    int psmind,
    bool second,
    std::vector<std::string>& flanking_aas
  );

};

#endif
