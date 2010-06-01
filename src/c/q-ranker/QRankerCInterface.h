/*******************************************************************************
 * Q-ranker unofficial version
 * Copyright (c) 2006-8 University of Washington. All rights reserved.
 * Written by Lukas K�ll (lukall@u.washington.edu) in the 
 * Department of Genome Science at the University of Washington. 
 * and Marina Spivak (spivak.marina@gmail.com) at
 * NEC Labs of America.
 *******************************************************************************/
#ifndef QRANKER_C_INTERFACE_H_
#define QRANKER_C_INTERFACE_H_

#include "PercolatorCommon.h"


/** Call that initiates percolator */
#include <vector>
void qcInitiate2(NSet sets, unsigned int numFeatures, 
  std::vector<unsigned int>& numSpectra, char ** featureNames, double pi0);

Caller * getCallerQR();


#ifdef __cplusplus
extern "C" {
#endif

/** Call that initiates percolator */
void qcInitiate(NSet sets, unsigned int numFeatures, unsigned int numSpectra, char ** featureNames, double pi0);
 
/** Call that sets verbosity level
 *  0 is quiet, 2 is default, 5 is more than you want */
void qcSetVerbosity(int verbosity);


/** Register a PSM */
void qcRegisterPSM(SetType set, char * identifier, double * features);

/** Function called when we want to start processing */
void qcExecute(); 

/**
 * Given the set enum and features, return the Percolator score for the PSM
 */
void qcScorePSM(
  SetType set, ///< The PSM tag -in
  double* features,  ///< the features -in
  double* score ///< output the Percolator score -out
  );

/** Function called when retrieving target scores and q-values after processing,
  * the array should be numSpectra long and will be filled in the same order
  * as the features were inserted */
void qcGetScores(double *scoreArr, double *qArr); 

/** Function that should be called after processing finished */
void qcCleanUp(); 


#ifdef __cplusplus
}
#endif
#endif /*QRanker_C_INTERFACE_H_*/
