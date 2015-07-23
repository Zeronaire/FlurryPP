/*!
 * \file solver_overset.cpp
 * \brief Overset-related methods for solver class
 *
 * \author - Jacob Crabill
 *           Aerospace Computing Laboratory (ACL)
 *           Aero/Astro Department. Stanford University
 *
 * \version 0.0.1
 *
 * Flux Reconstruction in C++ (Flurry++) Code
 * Copyright (C) 2015 Jacob Crabill.
 *
 */

#include "solver.hpp"

#include <sstream>
#include <omp.h>

#include "input.hpp"
#include "geo.hpp"

/* ---- My New Overset Grid Functions ---- */

void solver::oversetInterp(void)
{
  cout << "Grid " << Geo->gridID << ": Interpolating Overset Data" << endl;

  U_ipts.resize(Geo->nGrids);
  for (int g=0; g<Geo->nGrids; g++) {
    if (g == Geo->gridID) continue;
    for (int i=0; i<Geo->foundPts[g].size(); i++) {
      point refPos = Geo->foundLocs[g][i];
      int ic = Geo->foundEles[g][i];
      vector<double> U_ipt(params->nFields);
      opers[eles[ic].eType][eles[ic].order].interpolateToPoint(eles[ic].U_spts, U_ipt, refPos);
      U_ipts[g].insertRow(U_ipt);
    }
  }

  Geo->exchangeOversetData(U_ipts, U_opts);
}

void solver::setupOverset(void)
{
  if (gridRank == 0) cout << "Solver: Grid " << gridID << ": Setting up overset connectivity" << endl;

  overPts.resize(0);
  for (auto &oface: overFaces) {
    oface->fptOffset = overPts.size();
    auto pts = oface->getPosFpts();
    overPts.insert(overPts.begin(),pts.begin(),pts.end());
  }
  nOverPts = overPts.size();

  overPtsPhys = createMatrix(overPts);

  U_opts.setup(nOverPts,params->nFields);

  Geo->matchOversetPoints(eles, overFaces);
}

/* ---- Basic Tioga-Based Overset-Grid Functions ---- */

//void solver::setupOverset(void)
//{
//  if (gridRank == 0) cout << "Solver: Grid " << gridID << ": Setting up overset connectivity" << endl;
//  setupOversetData();
//  // Give TIOGA a pointer to this solver for access to callback functions
//  tg->setcallback(this);
//  Geo->updateOversetConnectivity();
//}

void solver::setupOversetData(void)
{
  // Allocate storage for global solution vector (for use with Tioga)
  // and initialize to 0
  int nSptsTotal = 0;
  for (uint i=0; i<eles.size(); i++) nSptsTotal += eles[i].getNSpts();
  U_spts.assign(nSptsTotal*params->nFields,0);
}

void solver::setGlobalSolutionArray(void)
{
  int ind = 0;
  for (uint i=0; i<eles.size(); i++) {
    eles[i].getUSpts(&U_spts[ind]);
    ind += eles[i].getNSpts() * params->nFields;
  }
}

void solver::updateElesSolutionArrays(void)
{
  int ind = 0;
  for (uint i=0; i<eles.size(); i++) {
    eles[i].setUSpts(&U_spts[ind]);
    ind += eles[i].getNSpts() * params->nFields;
  }
}

void solver::callDataUpdateTIOGA(void)
{
  //cout << "Calling dataUpdate_highorder" << endl;
  tg->dataUpdate_highorder(params->nFields,U_spts.data(),0);
}

/* ---- Callback Functions for TIOGA Overset Grid Library ---- */

void solver::getNodesPerCell(int* cellID, int* nNodes)
{
  (*nNodes) = eles[*cellID].getNSpts();
}

void solver::getReceptorNodes(int* cellID, int* nNodes, double* posNodes)
{
  if (*cellID >= eles.size()) cout << "Invalid cellID!  cellID = " << *cellID << endl;
  eles[*cellID].getPosSpts(posNodes);
}

void solver::donorInclusionTest(int* cellID, double* xyz, int* passFlag, double* rst)
{
  // Determine if point is in cell: [x,y,z] should all lie between [-1,1]
  point refPt;
  (*passFlag) = eles[*cellID].getRefLocNelderMeade(point(xyz),refPt);

  rst[0] = refPt.x;
  rst[1] = refPt.y;
  rst[2] = refPt.z;
}

void solver::donorWeights(int* cellID, double* xyz, int* nWeights, int* iNode, double* weights, double* rst, int* fracSize)
{
  int ic = *cellID;

  // Get starting offset for global solution-point index
  int iStart = 0;
  for (int i=0; i<ic; i++)
    iStart += eles[i].getNSpts();

  // Put the global indices of ele's spts into inode array
  (*nWeights) = eles[ic].getNSpts();
  for (int i=0; i<(*nWeights); i++)
    iNode[i] = iStart + i;

  opers[eles[ic].eType][eles[ic].order].getInterpWeights(rst,weights);
}

void solver::convertToModal(int* cellID, int* nPtsIn, double* uIn, int* nPtsOut, int* iStart, double* uOut)
{
  // This is apparently supposed to convert nodal data to modal data...
  // ...but I don't need it.  So, just copy the data over.

  int ic = *cellID;
  *nPtsOut = *nPtsIn;

  // Copy uIn to uOut
  uOut = uIn;

  // Get starting offset for global solution-point index
  *iStart = 0;
  for (int i=0; i<ic; i++)
    *iStart += eles[i].getNSpts();
}
