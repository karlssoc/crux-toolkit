/**
 * \file PrintProcessedSpectra.h
 *
 * AUTHOR: Barbara Frewen
 * CREATE DATE: September 18, 2009
 * DESCRIPTION: Main method for the print-processed-spectra command.
 *              For every spectrum in an ms2 file, process as for
 *              xcorr and print peaks in ms2 format to new file.
 * REVISION:
 */
#ifndef EXTRACTSCANNEUTRALMASS_H
#define EXTRACTSCANNEUTRALMASS_H
#include "CruxApplication.h"


#include "crux-utils.h"
#include "carp.h"
#include "parameter.h"
#include "SpectrumCollection.h"
#include "FilteredSpectrumChargeIterator.h"
#include "scorer.h"

#include <string>

class ExtractScanNeutralMass: public CruxApplication {

 public:

  ExtractScanNeutralMass();
  ~ExtractScanNeutralMass();
  virtual int main(int argc, char** argv);
  virtual std::string getName();
  virtual std::string getDescription();

};


#endif