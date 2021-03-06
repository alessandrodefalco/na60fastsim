#if !defined(__CINT__) || defined(__MAKECINT__)
#include <Riostream.h>
#include <TString.h>
#include <TTree.h>
#include <TArrayF.h>
#include <TMath.h>
#include <TH1F.h>
#include <TNtuple.h>
#include <TFile.h>
#include "KMCProbeFwd.h"
#include "KMCDetectorFwd.h"
#include "TLorentzVector.h"
#include "TGenPhaseSpace.h"
#include "TRandom.h"
#include "TF1.h"
#include "THnSparse.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "TFile.h"
#include "TROOT.h"
#include "GenMUONLMR.h"
#include "TStopwatch.h"
#include "TTree.h"
#include "TParticle.h"
#include "AliAODRecoDecay.h"
#include "AliDecayer.h"
#include "AliDecayerEvtGen.h"
#include "TDatabasePDG.h"

#endif

// Track Chi2Tot cut
double ChiTot = 1.5;

// settings for signal generation
double yminSG = -10.; // min y to generate
double ymaxSG = 10.;  //
double ptminSG = 0.;
double ptmaxSG = 10; //Elena's change, it was 3 GeV/c

double vX = 0, vY = 0, vZ = 0; // event vertex

THnSparseF* CreateSparse();
void ComputeVertex(KMCProbeFwd &t0, KMCProbeFwd &t1, Double_t &xV, Double_t &yV, Double_t &zV);
Double_t CosPointingAngle(Double_t vprim[3], Double_t vsec[3], TLorentzVector &parent);
TDatime dt;
Double_t ImpParXY(Double_t vprim[3], Double_t vsec[3], TLorentzVector &parent);
Double_t CosThetaStar(TLorentzVector &parent, TLorentzVector &dauk);

void GenerateD0SignalCandidates(Int_t nevents = 100000, 
			   double Eint = 160., 
			   const char *setup = "setup-10um-itssa_Eff1.txt", 
			   const char *filNamPow="/home/prino/na60plus/POWHEG/pp20/Charm1dot5/pp0_frag-PtSpectra-Boost.root", 
			   const char *privateDecayTable = "/home/prino/na60plus/decaytables/USERTABD0.DEC", 
			   bool writeNtuple = kFALSE, 
			   bool simulateBg=kTRUE){
  
  // Generate D0->Kpi signals and simulate detector response for decay tracks
  // Need input D0 pt and y ditribution from POWHEG


  int refreshBg = 1000;
  static UInt_t seed = dt.Get();
  gRandom->SetSeed(seed);
  gSystem->Load("$ALICE_ROOT/lib/libEvtGen.so");
  gSystem->Load("$ALICE_ROOT/lib/libEvtGenExternal.so");
  gSystem->Load("$ALICE_ROOT/lib/libTEvtGen.so");
  

  //  PYTHIA input -> not used
  // TFile *fin = new TFile("Mergedfkine.root");
  // // TFile *fin = new TFile("fkineNew.root");
  // TH1D *hD0pt = (TH1D *)fin->Get("hD0pt");
  // TH1D *hD0y = (TH1D *)fin->Get("hD0y");
  // hD0y->Rebin(4);
  
  //  POWHEG+PYTHIA input 
  printf("--> pt and y shape of D0 from %s\n",filNamPow);
  TFile *filPow=new TFile(filNamPow);
  TH3D* h3Dpow=(TH3D*)filPow->Get("hptyeta421");
  TH1D *hD0pt = (TH1D*)h3Dpow->ProjectionX("hD0pt");
  TH1D *hD0y = (TH1D*)h3Dpow->ProjectionY("hD0y");
  
  TH2F *hptK = new TH2F("hptK", "kaons from D0 decays", 50,0.,10.,50, 0., 10.);
  TH2F *hptPi = new TH2F("hptPi", "pions from D0 decays", 50, 0.,10.,50,0., 10.);
  TH1F *hyK = new TH1F("hyK", "y kaons from D0 decays", 50, 0., 5.);
  TH1F *hyPi = new TH1F("hyPi", "y pions from D0 decays", 50, 0., 5.);
  TH2F *hyPiK = new TH2F("hyPiK", "y pions vs y Kaons from D0 decays", 50, 0., 5., 50, 0., 5.);
  
  //Magnetic field and detector parameters
  MagField *mag = new MagField(1);
  int BNreg = mag->GetNReg();
  const double *BzMin = mag->GetZMin();
  const double *BzMax = mag->GetZMax();
  const double *BVal;
  printf("*************************************\n");
  printf("number of magnetic field regions = %d\n", BNreg);
  
  TFile *fout = new TFile("Matching-histos.root", "recreate");
  
  for (int i = 0; i < BNreg; i++){
    BVal = mag->GetBVals(i);
    printf("*** Field region %d ***\n", i);
    if (i == 0){
      printf("Bx = %f B = %f Bz = %f zmin = %f zmax = %f\n", BVal[0], BVal[1], BVal[2], BzMin[i], BzMax[i]);
    }else if (i == 1){
      printf("B = %f Rmin = %f Rmax = %f zmin = %f zmax = %f\n", BVal[0], BVal[1], BVal[2], BzMin[i], BzMax[i]);
    }
  }
  
  //int outN = nev/10;
  //if (outN<1) outN=1;

  KMCDetectorFwd *det = new KMCDetectorFwd();
  det->ReadSetup(setup, setup);
  det->InitBkg(Eint);
  det->ForceLastActiveLayer(det->GetLastActiveLayerITS()); // will not propagate beyond VT

  det->SetMinITSHits(det->GetNumberOfActiveLayersITS()); //NA60+
  //det->SetMinITSHits(det->GetNumberOfActiveLayersITS()-1); //NA60
  det->SetMinMSHits(0); //NA60+
  //det->SetMinMSHits(det->GetNumberOfActiveLayersMS()-1); //NA60
  det->SetMinTRHits(0);
  //
  // max number of seeds on each layer to propagate (per muon track)
  det->SetMaxSeedToPropagate(3000);
  //
  // set chi2 cuts
  det->SetMaxChi2Cl(10.);  // max track to cluster chi2
  det->SetMaxChi2NDF(3.5); // max total chi2/ndf
  det->SetMaxChi2Vtx(20);  // fiducial cut on chi2 of convergence to vtx  
  // IMPORTANT FOR NON-UNIFORM FIELDS
  det->SetDefStepAir(1);
  det->SetMinP2Propagate(1); //NA60+
  //det->SetMinP2Propagate(2); //NA60
  //
  det->SetIncludeVertex(kFALSE); // count vertex as an extra measured point
  //  det->SetApplyBransonPCorrection();
  det->ImposeVertex(0., 0., 0.);
  det->BookControlHistos();
  //
  
  
  // prepare decays
  TGenPhaseSpace decay;
  TLorentzVector parentgen, daugen[2], parent, daurec[2]; //eli
  KMCProbeFwd recProbe[2];  
  AliDecayerEvtGen *fDecayer = new AliDecayerEvtGen();
  fDecayer->Init(); //read the default decay table DECAY.DEC and particle table
  bool privTab=kFALSE;
  if (strlen(privateDecayTable)>0){
    if(gSystem->Exec(Form("ls -l %s",privateDecayTable))==0){
      fDecayer->SetDecayTablePath((char*)privateDecayTable);
      fDecayer->ReadDecayTable();
      printf("-- Use D0 decay table from file %s\n",privateDecayTable);
      privTab=kTRUE;
    }
  }
  if(!privTab){
    printf("-- Use existing decay modes in aliroot\n");
    fDecayer->SetForceDecay(kHadronicD); 
  }
  fDecayer->ForceDecay();
  
  TClonesArray *particles = new TClonesArray("TParticle", 1000);
  TLorentzVector *mom = new TLorentzVector();
  
  // define mother particle
  Int_t pdgParticle = 421;
  
  TH2F* hYPtGen = new TH2F("YPTGen", "Y-Pt corr match", 80, 1.0, 5.4, 40, ptminSG, ptmaxSG);
  TH1F* hPtGen = new TH1F("PTGen", "Pt gen", 40, ptminSG, ptmaxSG);
  TH1F* hYGen = new TH1F("hYGen", "Y full phase space", 80., 1., 5.4);
  TH2F* hYPtFake = new TH2F("YPTFake", "Y-Pt fake match", 80, 1.0, 5.4, 40, ptminSG, ptmaxSG);
  TH2F* hYPtAll = new TH2F("YPTAll", "Y-Pt all match", 80, 1.0, 5.4, 40, ptminSG, ptmaxSG);
  TH1F* hPtFake = new TH1F("PTFake", "Pt fake match", 40, ptminSG, ptmaxSG);
  TH1F* hPtAll = new TH1F("PTAll", "Pt all match", 40, ptminSG, ptmaxSG);  
  TH1F* hMassFake = new TH1F("MassFake", "Mass fake match", 200, 1., 3.5);
  TH1F* hMassAll = new TH1F("MassAll", "Mass all match", 200, 1., 3.5);

  TH2F *hDistXY = new TH2F("hDistXY", "", 100, 0, 0.1, 30, 0, 3);
  TH2F *hDist = new TH2F("hDist", "", 300, 0, 10, 30, 0, 3);
  TH2F *hDistgenXY = new TH2F("hDistgenXY", "", 100, 0, 0.1, 30, 0, 3);
  TH2F *hDistgen = new TH2F("hDistgen", "", 300, 0, 10, 30, 0, 3);
  TH2F *hCosp = new TH2F("hCosp", "", 300, -1, 1, 30, 0, 3);
  TH2F *hDCA = new TH2F("hDCA", "", 100, 0, 0.1, 30, 0, 3);
  TH2F *hDCAx = new TH2F("hDCAx", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hDCAy = new TH2F("hDCAy", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hDCAz = new TH2F("hDCAz", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hd0XYprod = new TH2F("hd0xyprod", "", 100, -0.01, 0.01, 30, 0, 3);
  TH2F *hd0XY1 = new TH2F("hd0xy1", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hd0XY2 = new TH2F("hd0xy2", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hMassVsPt = new TH2F("hMassVsPt", "", 200, 1.5, 2.5, 6, 0, 3);
  TH2F *hMassVsY = new TH2F("hMassVsY", "", 200, 1.5, 2.5, 10, 0, 5);
  
  TH2F *hResVx = new TH2F("hResVx", "", 200, -1000., 1000., 30, 0, 3);
  TH2F *hResVy = new TH2F("hResVy", "", 200, -1000., 1000., 30, 0, 3);
  TH2F *hResVz = new TH2F("hResVz", "", 200, -1000., 1000., 30, 0, 3);
  TH2F *hResPx = new TH2F("hResPx", "", 100, -1, 1, 30, 0, 3); //for Kaons
  TH2F *hResPy = new TH2F("hResPy", "", 100, -1, 1, 30, 0, 3);
  TH2F *hResPz = new TH2F("hResPz", "", 100, -1, 1, 30, 0, 3);
  TH2F *hResVxVsY = new TH2F("hResVxVsY", "", 200, -1000., 1000., 50, 0, 5);
  TH2F *hResVyVsY = new TH2F("hResVyVsY", "", 200, -1000., 1000., 30, 0, 5);
  TH2F *hResVzVsY = new TH2F("hResVzVsY", "", 200, -1000., 1000., 30, 0, 5);
  TH2F *hResPxVsY = new TH2F("hResPxVsY", "", 100, -1, 1, 50, 0, 5); //for Kaons
  TH2F *hResPyVsY = new TH2F("hResPyVsY", "", 100, -1, 1, 50, 0, 5);
  TH2F *hResPzVsY = new TH2F("hResPzVsY", "", 100, -1, 1, 50, 0, 5);
  TH2F *hResDist = new TH2F("hResDist", "", 100, -0.5, 0.5, 30, 0, 3);
  TH2F *hResDistXY = new TH2F("hResDistXY", "", 100, -0.1, 0.1, 30, 0, 3);
  TH1F *hNevents = new TH1F("hNevents", "", 1, 0, 1);
  
  TH2F *hd0 = new TH2F("hd0", "", 100, 0, 0.1, 30, 0, 3);
  THnSparseF *hsp = CreateSparse();
  const Int_t nDim=static_cast<const Int_t>(hsp->GetNdimensions());
  Double_t arrsp[nDim];

  TFile *fnt = 0x0;
  TNtuple *ntD0cand = 0x0;
  if (writeNtuple)
    {
      fnt = new TFile("fntSig.root", "recreate");
      ntD0cand = new TNtuple("ntD0cand", "ntD0cand", "mass:pt:dist:cosp:d01:d02:d0prod:dca:ptMin:ptMax", 32000);
    }
  Float_t arrnt[10];
  for (Int_t iev = 0; iev < nevents; iev++){
    hNevents->Fill(0.5);
    Double_t vprim[3] = {0, 0, 0};
    if(iev%100==0) printf(" ***************  ev = %d \n", iev);
    int nrec = 0;
    int nfake = 0;
    double pxyz[3];
    
    if (simulateBg && (iev%refreshBg)==0) det->GenBgEvent(0.,0.,0.);
    Double_t ptGenD = hD0pt->GetRandom(); // get D0 distribution from file
    Double_t yGenD = hD0y->GetRandom();
    Double_t phi = gRandom->Rndm() * 2 * TMath::Pi();
    Double_t pxGenD = ptGenD * TMath::Cos(phi);
    Double_t pyGenD = ptGenD * TMath::Sin(phi);
    
    Double_t mass = TDatabasePDG::Instance()->GetParticle(pdgParticle)->Mass();
    Double_t mt = TMath::Sqrt(ptGenD * ptGenD + mass * mass);
    Double_t pzGenD = mt * TMath::SinH(yGenD);
    Double_t en = mt * TMath::CosH(yGenD);
    
    mom->SetPxPyPzE(pxGenD, pyGenD, pzGenD, en);
    Int_t np;
    do{
      fDecayer->Decay(pdgParticle, mom);
      np = fDecayer->ImportParticles(particles);
    } while (np < 0);
    
    Int_t arrpdgdau[2];
    Double_t ptK = -999.;
    Double_t ptPi = -999.;
    Double_t yK=-999.;
    Double_t yPi = -999.;
    Int_t icount = 0;
    Double_t secvertgenK[3]={0.,0.,0.};
    Double_t secvertgenPi[3]={0.,0.,0.};
    
    // loop on decay products
    for (int i = 0; i < np; i++) { 
      TParticle *iparticle1 = (TParticle *)particles->At(i);
      Int_t kf = TMath::Abs(iparticle1->GetPdgCode());
      vX = iparticle1->Vx();
      vY = iparticle1->Vy();
      vZ = iparticle1->Vz();
      if (kf == pdgParticle){
	// D0 particle
	hYGen->Fill(iparticle1->Y());
	hPtGen->Fill(iparticle1->Pt());
	hYPtGen->Fill(iparticle1->Y(), iparticle1->Pt());
	//printf("mother part = %d, code=%d, pt=%f, y=%f \n", i, kf, iparticle1->Pt(), iparticle1->Y());
      }	
      if (kf == 321 || kf == 211){
	// daughters that can be reconstructed: K and pi
	Double_t e = iparticle1->Energy();
	Double_t px = iparticle1->Px();
	Double_t py = iparticle1->Py();
	Double_t pz = iparticle1->Pz();	    
	TLorentzVector *pDecDau = new TLorentzVector(0., 0., 0., 0.);
	pDecDau->SetXYZM(iparticle1->Px(), iparticle1->Py(), iparticle1->Pz(), iparticle1->GetMass());
	Int_t crg=1;
	if(iparticle1->GetPdgCode()<0) crg=-1;
	if (!det->SolveSingleTrack(pDecDau->Pt(), pDecDau->Rapidity(), pDecDau->Phi(), iparticle1->GetMass(), crg, vX, vY, vZ, 0, 1, 99)) continue;
	KMCProbeFwd *trw = det->GetLayer(0)->GetWinnerMCTrack();
	if (!trw) continue;
	if (trw->GetNormChi2(kTRUE) > ChiTot) continue;
	nrec++;
	    
	nfake += trw->GetNFakeITSHits();
	trw->GetPXYZ(pxyz);
	if (kf == 321){
	  // Kaon daughter
	  ptK = iparticle1->Pt();
	  yK = iparticle1->Y();
	  hptK->Fill(ptGenD,ptK);
	  hyK->Fill(iparticle1->Y());
	  secvertgenK[0] = iparticle1->Vx();
	  secvertgenK[1] = iparticle1->Vy();
	  secvertgenK[2] = iparticle1->Vz();
	  daugen[0].SetXYZM(iparticle1->Px(), iparticle1->Py(), iparticle1->Pz(), iparticle1->GetMass());
	  daurec[0].SetXYZM(pxyz[0], pxyz[1], pxyz[2], iparticle1->GetMass());
	  recProbe[0] = *trw;
	}else if (kf == 211){
	  // Pion daughter
	  ptPi = iparticle1->Pt();
	  yPi = iparticle1->Y();
	  hptPi->Fill(ptGenD,ptPi);
	  hyPi->Fill(iparticle1->Y());
	  secvertgenPi[0] = iparticle1->Vx();
	  secvertgenPi[1] = iparticle1->Vy();
	  secvertgenPi[2] = iparticle1->Vz();
	  daugen[1].SetXYZM(iparticle1->Px(), iparticle1->Py(), iparticle1->Pz(), iparticle1->GetMass());
	  daurec[1].SetXYZM(pxyz[0], pxyz[1], pxyz[2],  iparticle1->GetMass());
	  recProbe[1] = *trw;
	}
      }
    }
    if (ptK > 0 && ptPi > 0) hyPiK->Fill(yPi, yK);
    if (nrec < 2) continue;
    
    recProbe[0].PropagateToDCA(&recProbe[1]);
    
    parent = daurec[0];
    parent += daurec[1];
    parentgen = daugen[0];
    parentgen += daugen[1];

    Double_t  ptRecD=parent.Pt();
    Double_t  massRecD=parent.M();
    Double_t yRecD = 0.5 * TMath::Log((parent.E() + parent.Pz()) / (parent.E() - parent.Pz()));
    hYPtAll->Fill(yRecD, ptRecD);
    hPtAll->Fill(ptRecD);
    hMassAll->Fill(massRecD);
    if (nfake > 0){
      hYPtFake->Fill(yRecD, ptRecD);
      hPtFake->Fill(ptRecD);
      hMassFake->Fill(massRecD);
    }
    hMassVsPt->Fill(massRecD,ptRecD);
    hMassVsY->Fill(massRecD,yRecD);
    
    Float_t d1 = recProbe[1].GetX() - recProbe[0].GetX();
    Float_t d2 = recProbe[1].GetY() - recProbe[0].GetY();
    Float_t d3 = recProbe[1].GetZ() - recProbe[0].GetZ();
    Float_t dca = sqrt(d1 * d1 + d2 * d2 + d3 * d3);
    // printf(" DCA = %f\n", sqrt(d1 * d1 + d2 * d2 + d3 * d3));
    hDCA->Fill(dca, ptRecD);
    hDCAx->Fill(d1, ptRecD);
    hDCAy->Fill(d2, ptRecD);
    hDCAz->Fill(d3, ptRecD);
    
    //      Double_t xP = (recProbe[1].GetX() + recProbe[0].GetX()) / 2.;
    //      Double_t yP = (recProbe[1].GetY() + recProbe[0].GetY()) / 2.;
    //      Double_t zP = (recProbe[1].GetZ() + recProbe[0].GetZ()) / 2.;
    
    Double_t xP, yP, zP;
    ComputeVertex(recProbe[0],recProbe[1],xP,yP,zP);
    Double_t residVx=10000.*(xP - secvertgenK[0]);
    Double_t residVy=10000.*(yP - secvertgenK[1]);
    Double_t residVz=10000.*(zP - secvertgenK[2]);
    hResVx->Fill(residVx, ptRecD);
    hResVy->Fill(residVy, ptRecD);
    hResVz->Fill(residVz, ptRecD);
    hResVxVsY->Fill(residVx, yRecD);
    hResVyVsY->Fill(residVy, yRecD);
    hResVzVsY->Fill(residVz, yRecD);
    
    hResPx->Fill(daurec[0].Px() - daugen[0].Px(), ptRecD);
    hResPy->Fill(daurec[0].Py() - daugen[0].Py(), ptRecD);
    hResPz->Fill(daurec[0].Pz() - daugen[0].Pz(), ptRecD);
    hResPx->Fill(daurec[1].Px() - daugen[1].Px(), ptRecD);
    hResPy->Fill(daurec[1].Py() - daugen[1].Py(), ptRecD);
    hResPz->Fill(daurec[1].Pz() - daugen[1].Pz(), ptRecD);

    hResPxVsY->Fill(daurec[0].Px() - daugen[0].Px(), yRecD);
    hResPyVsY->Fill(daurec[0].Py() - daugen[0].Py(), yRecD);
    hResPzVsY->Fill(daurec[0].Pz() - daugen[0].Pz(), yRecD);
    hResPxVsY->Fill(daurec[1].Px() - daugen[1].Px(), yRecD);
    hResPyVsY->Fill(daurec[1].Py() - daugen[1].Py(), yRecD);
    hResPzVsY->Fill(daurec[1].Pz() - daugen[1].Pz(), yRecD);
    
    // cout << "secvert generated Pion: " << secvertgenPi[0] << "  " << secvertgenPi[1] << "  " << secvertgenPi[2] << endl;
    // cout << "Reco Vert  Pion: " << xP << "  " << yP << "  " << zP << endl;
    
    Float_t dist = TMath::Sqrt(xP * xP + yP * yP + zP * zP);
    Float_t distXY = TMath::Sqrt(xP * xP + yP * yP);
    Float_t distgen = TMath::Sqrt(secvertgenPi[0] * secvertgenPi[0] + secvertgenPi[1] * secvertgenPi[1] + secvertgenPi[2] * secvertgenPi[2]);
    Float_t distgenXY = TMath::Sqrt(secvertgenPi[0] * secvertgenPi[0] + secvertgenPi[1] * secvertgenPi[1]);
    // printf("dist = %f , distXY=%f , dx=%f, dy=%f, dz=%f z1=%f, z2=%f \n", dist, distXY, xP, yP, zP, recProbe[0].GetZ(), recProbe[1].GetZ());
    // printf("distgen = %f , distgenXY=%f \n", distgen, distgenXY);
    
    Double_t vsec[3] = {xP, yP, zP};
    Double_t cosp = CosPointingAngle(vprim, vsec, parent);
    Double_t cts = CosThetaStar(parent,daurec[0]);
    Double_t ipD = ImpParXY(vprim, vsec, parent);
    hCosp->Fill(cosp, ptRecD);
    // printf(" ***** ***** cos point = %f \n", cosp);
    //if (cosp < -0.98)
    //    printf("SMALL COSPOINT");
    
    hResDist->Fill(dist - distgen, ptRecD);
    hResDistXY->Fill(distXY - distgenXY, ptRecD);
    
    //recProbe[0].PropagateToDCA(&recProbe[1]);
    
    // hYPtAll->Fill(parent.Y(), ptRecD);
    // hPtAll->Fill(ptRecD);
    hDistXY->Fill(distXY, ptRecD);
    hDist->Fill(dist, ptRecD);
    hDistgenXY->Fill(distgenXY, ptRecD);
    hDistgen->Fill(distgen, ptRecD);
      
    //AliExternalTrackParam *track1 = (AliExternalTrackParam *)recProbe[0].GetTrack();
    //AliExternalTrackParam *track2 = (AliExternalTrackParam *)recProbe[1].GetTrack();
    recProbe[0].PropagateToZBxByBz(0);
    Double_t d0x1 = recProbe[0].GetX();
    Double_t d0y1 = recProbe[0].GetY();
    Double_t d0xy1 = TMath::Sqrt(d0x1 * d0x1 + d0y1 * d0y1);
    if (d0x1 < 0)
      d0xy1 *= -1;
    
    recProbe[1].PropagateToZBxByBz(0);
    Double_t d0x2 = recProbe[1].GetX();
    Double_t d0y2 = recProbe[1].GetY();
    Double_t d0xy2 = TMath::Sqrt(d0x2 * d0x2 + d0y2 * d0y2);
    if (d0x2 < 0)
      d0xy2 *= -1;
    
    // printf("d0xy1 = %f, d0xy2 = %f \n", d0xy1, d0xy2);
    
    hd0XYprod->Fill(d0xy1 * d0xy2, ptRecD);
    hd0XY1->Fill(d0xy1, ptRecD);
    hd0XY2->Fill(d0xy2, ptRecD);
      
    arrsp[0] = massRecD;
    arrsp[1] = ptRecD;
    arrsp[2] = dist;
    arrsp[3] = cosp;
    arrsp[4] = TMath::Min(TMath::Abs(d0xy1),TMath::Abs(d0xy2));
    arrsp[5] = d0xy1 * d0xy2;
    arrsp[6] = dca;
    arrsp[7] = TMath::Min(recProbe[0].GetTrack()->Pt(),recProbe[1].GetTrack()->Pt());
    arrsp[8] = TMath::Abs(ipD);	    
    arrsp[9] = cts;      
    hsp->Fill(arrsp);
    
    if (ntD0cand){
      arrnt[0] = massRecD;
      arrnt[1] = ptRecD;
      arrnt[2] = dist;
      arrnt[3] = cosp;
      arrnt[4] = d0xy1;
      arrnt[5] = d0xy2;
      arrnt[6] = d0xy1 * d0xy2;
      arrnt[7] = dca;
      arrnt[8] = TMath::Min(recProbe[0].GetTrack()->Pt(),recProbe[1].GetTrack()->Pt());
      arrnt[9] = TMath::Max(recProbe[0].GetTrack()->Pt(),recProbe[1].GetTrack()->Pt());
      ntD0cand->Fill(arrnt);
    }
  } //event loop
  
  hMassAll->SetLineColor(kBlue);
  hMassAll->Draw();
  hMassAll->SetMinimum(0.1);
  hMassFake->SetLineColor(kRed);
  hMassFake->Draw("same");
  
  fout->cd();  
  hMassAll->Write();
  hMassFake->Write();
  hMassVsPt->Write();
  hMassVsY->Write();
  hYPtGen->Write();
  hYPtAll->Write();
  hYPtFake->Write();
  hYGen->Write();
  hPtGen->Write();
  hPtAll->Write();
  hPtFake->Write();
  hDistXY->Write();
  hDist->Write();
  hDistgenXY->Write();
  hDistgen->Write();
  hCosp->Write();
  hDCA->Write();
  hDCAx->Write();
  hDCAy->Write();
  hDCAz->Write();
  hResVx->Write();
  hResVy->Write();
  hResVz->Write();
  hResPx->Write();
  hResPy->Write();
  hResPz->Write();
  hResVxVsY->Write();
  hResVyVsY->Write();
  hResVzVsY->Write();
  hResPxVsY->Write();
  hResPyVsY->Write();
  hResPzVsY->Write();
  hResDist->Write();
  hResDistXY->Write();
  hd0XYprod->Write();
  hd0XY1->Write();
  hd0XY2->Write();
  hNevents->Write();
  hsp->Write();
  if (ntD0cand){
    fnt->cd();
    ntD0cand->Write();
    fnt->Close();
  }
  // TCanvas *ccdau = new TCanvas();
  // ccdau->Divide(3, 2);
  // ccdau->cd(1)->SetLogy();
  // hPtGen->Draw();
  // ccdau->cd(2)->SetLogy();
  // hptK->Draw();
  // ccdau->cd(3)->SetLogy();
  // hptPi->Draw();
  // ccdau->cd(4)->SetLogy();
  // hYGen->Draw();
  // ccdau->cd(5)->SetLogy();
  // hyK->Draw();
  // ccdau->cd(6)->SetLogy();
  // hyPi->Draw();
  
  TFile fout2("DecayHistos.root", "RECREATE");
  // TFile fout2("DecayHistostest.root", "RECREATE");
  hPtGen->Write();
  hptK->Write();
  hptPi->Write();
  hyK->Write();
  hyPi->Write();
  hYGen->Write();
  hyPiK->Write();
  fout2.Close();

  fout->Close();
}



void MakeD0CombinBkgCandidates(const char* trackTreeFile="treeBkgEvents.root",
			       Int_t nevents = 999999, 
			       Int_t writeNtuple = kFALSE){

  // Read the TTree of tracks produced with runBkgVT.C
  // Create D0 combinatorial background candidates (= OS pairs of tracks)
  // Store in THnSparse and (optionally) TNtuple

  TFile *filetree = new TFile(trackTreeFile);
  TTree *tree = (TTree *)filetree->Get("tree");
  TClonesArray *arr = 0;
  tree->SetBranchAddress("tracks", &arr);
  Int_t entries = tree->GetEntries();
  printf("Number of events in tree = %d\n",entries);
  if(nevents>entries) nevents=entries;
  else printf(" --> Analysis performed on first %d events\n",nevents);

  TDatime dt;
  static UInt_t seed = dt.Get();
  gRandom->SetSeed(seed);

  // define mother particle
  Int_t pdgParticle = 421;
  
  TFile *fout = new TFile("Matching-histos.root", "recreate");
  TH1F* hPtAll = new TH1F("PTAll", "Pt all match", 50, 0., 5.);
  TH2F* hYPtAll = new TH2F("YPTAll", "Y-Pt all match", 80, 1.0, 5.4, 50, 0., 5.);
  TH1F* hMassAll = new TH1F("MassAll", "Mass all match", 250, 0., 2.5);

  TH2F *hDistXY = new TH2F("hDistXY", "", 100, 0, 0.1, 30, 0, 3);
  TH2F *hDist = new TH2F("hDist", "", 300, 0, 10, 30, 0, 3);
  TH2F *hDistgenXY = new TH2F("hDistgenXY", "", 100, 0, 0.1, 30, 0, 3);
  TH2F *hDistgen = new TH2F("hDistgen", "", 300, 0, 10, 30, 0, 3);
  TH2F *hDCA = new TH2F("hDCA", "", 100, 0, 0.1, 30, 0, 3);
  TH2F *hDCAx = new TH2F("hDCAx", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hDCAy = new TH2F("hDCAy", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hDCAz = new TH2F("hDCAz", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hd0XYprod = new TH2F("hd0xyprod", "", 100, -0.01, 0.01, 30, 0, 3);
  TH2F *hd0XY1 = new TH2F("hd0xy1", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hd0XY2 = new TH2F("hd0xy2", "", 100, -0.1, 0.1, 30, 0, 3);
  
  TH2F *hResVx = new TH2F("hResVx", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hResVy = new TH2F("hResVy", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hResVz = new TH2F("hResVz", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hResPx = new TH2F("hResPx", "", 100, -1, 1, 30, 0, 3); //for Kaons
  TH2F *hResPy = new TH2F("hResPy", "", 100, -1, 1, 30, 0, 3);
  TH2F *hResPz = new TH2F("hResPz", "", 100, -1, 1, 30, 0, 3);
  TH2F *hResDist = new TH2F("hResDist", "", 100, -0.5, 0.5, 30, 0, 3);
  TH2F *hResDistXY = new TH2F("hResDistXY", "", 100, -0.1, 0.1, 30, 0, 3);
  TH2F *hCosp = new TH2F("hCosp", "", 100, -1, 1, 30, 0, 3);
  TH2F *hCosThStVsMass = new TH2F("hCosThStVsMass", "", 50, 1.5, 2.5, 40, -1, 1);
  
  TH2F *hd0 = new TH2F("hd0", "", 100, 0, 0.1, 30, 0, 3);
  
  TH1F *hcand = new TH1F("hcand", "", 1000, 0, 500000);
  TH1F *hcandpeak = new TH1F("hcandpeak", "", 500, 0, 15000);
  TH1F *hNevents = new TH1F("hNevents", "", 1, 0, 1);
    
  THnSparseF *hsp = CreateSparse();
  const Int_t nDim=static_cast<const Int_t>(hsp->GetNdimensions());
  Double_t arrsp[nDim];

  TFile *fnt = 0x0;
  TNtuple *ntD0cand = 0x0;
  Float_t arrnt[10];
  if (writeNtuple){
    fnt = new TFile("fntBkg.root", "recreate");
    ntD0cand = new TNtuple("ntD0cand", "ntD0cand", "mass:pt:dist:cosp:d01:d02:d0prod:dca:ptMin:ptMax", 32000);
  }

  KMCProbeFwd recProbe[2];
  TLorentzVector parent, daurec[2];
  Double_t massD0 = 1.864;
  for (Int_t iev = 0; iev < nevents; iev++){
    hNevents->Fill(0.5);
    Double_t vprim[3] = {0, 0, 0};
    Double_t countCandInPeak = 0;
    Double_t countCand = 0;
    tree->GetEvent(iev);
    Int_t arrentr = arr->GetEntriesFast();
    
    for (Int_t itr = 0; itr < arrentr; itr++){
      KMCProbeFwd *tr1 = (KMCProbeFwd *)arr->At(itr);
      // cout << "tr P=" << tr1->GetP() << endl;
      Float_t ch1 = tr1->GetCharge();
      for (Int_t itr2 = itr; itr2 < arrentr; itr2++){
	KMCProbeFwd *tr2 = (KMCProbeFwd *)arr->At(itr2);
	Float_t ch2 = tr2->GetCharge();
	if (ch1 * ch2 > 0) continue;
	if (ch1 < 0){ //convention: first track negative
	  recProbe[0] = *tr1;
	  recProbe[1] = *tr2;
	}else if (ch2 < 0){
	  recProbe[0] = *tr2;
	  recProbe[1] = *tr1;
	}
	recProbe[0].PropagateToDCA(&recProbe[1]);
	Double_t pxyz[3];
	recProbe[0].GetPXYZ(pxyz);
	
	Double_t pxyz2[3];
	recProbe[1].GetPXYZ(pxyz2);
	
	for(Int_t iMassHyp=0; iMassHyp<2; iMassHyp++){
	  // mass hypothesis: Kpi, piK
	  Int_t iKaon=-1;
	  if(iMassHyp==0){
	    daurec[0].SetXYZM(pxyz[0], pxyz[1], pxyz[2], KMCDetectorFwd::kMassK);
	    daurec[1].SetXYZM(pxyz2[0], pxyz2[1], pxyz2[2], KMCDetectorFwd::kMassPi);
	    iKaon=0;
	  }else{
	    daurec[0].SetXYZM(pxyz[0], pxyz[1], pxyz[2], KMCDetectorFwd::kMassPi);
	    daurec[1].SetXYZM(pxyz2[0], pxyz2[1], pxyz2[2], KMCDetectorFwd::kMassK);
	    iKaon=1;
	  }
	  parent = daurec[0];
	  parent += daurec[1];
	  countCand++;
	  Float_t ptD=parent.Pt();
	  Float_t invMassD=parent.M();
	  Float_t yD = 0.5 * TMath::Log((parent.E() + parent.Pz()) / (parent.E() - parent.Pz()));
	  hYPtAll->Fill(yD, ptD);
	  hPtAll->Fill(ptD);
	  hMassAll->Fill(invMassD);
	  if(invMassD>1.65  && invMassD<2.15){
	    // range to fill histos
	    if(invMassD>1.805 && invMassD<1.925) countCandInPeak++;
	    Float_t d1 = recProbe[1].GetX() - recProbe[0].GetX();
	    Float_t d2 = recProbe[1].GetY() - recProbe[0].GetY();
	    Float_t d3 = recProbe[1].GetZ() - recProbe[0].GetZ();
	    Float_t dca = sqrt(d1 * d1 + d2 * d2 + d3 * d3);
	    
	    //printf(" DCA = %f\n", sqrt(d1 * d1 + d2 * d2 + d3 * d3));
	    hDCA->Fill(dca, ptD);
	    hDCAx->Fill(d1, ptD);
	    hDCAy->Fill(d2, ptD);
	    hDCAz->Fill(d3, ptD);
	    // Float_t xP = (recProbe[1].GetX() + recProbe[0].GetX()) / 2.;
	    // Float_t yP = (recProbe[1].GetY() + recProbe[0].GetY()) / 2.;
	    // Float_t zP = (recProbe[1].GetZ() + recProbe[0].GetZ()) / 2.;
	    Double_t xP, yP, zP;
	    ComputeVertex(recProbe[0],recProbe[1],xP,yP,zP);
	    Float_t dist = TMath::Sqrt(xP * xP + yP * yP + zP * zP);
	    Float_t distXY = TMath::Sqrt(xP * xP + yP * yP);
	    Double_t vsec[3] = {xP, yP, zP};
	    Double_t cosp = CosPointingAngle(vprim, vsec, parent);
	    Double_t cts = CosThetaStar(parent,daurec[iKaon]);
	    Double_t ipD = ImpParXY(vprim, vsec, parent);
	    hCosp->Fill(cosp, ptD);
	    hCosThStVsMass->Fill(invMassD,cts);
	    //printf(" ***** ***** cos point = %f \n", cosp);	    
	    hDistXY->Fill(distXY, ptD);
	    hDist->Fill(dist, ptD);
	    
	    recProbe[0].PropagateToZBxByBz(0);
	    Double_t d0x1 = recProbe[0].GetX();
	    Double_t d0y1 = recProbe[0].GetY();
	    Double_t d0xy1 = TMath::Sqrt(d0x1 * d0x1 + d0y1 * d0y1);
	    if (d0x1 < 0) d0xy1 *= -1;
	    
	    recProbe[1].PropagateToZBxByBz(0);
	    Double_t d0x2 = recProbe[1].GetX();
	    Double_t d0y2 = recProbe[1].GetY();
	    Double_t d0xy2 = TMath::Sqrt(d0x2 * d0x2 + d0y2 * d0y2);
	    if (d0x2 < 0) d0xy2 *= -1;
	      
	    //printf("d0xy1 = %f, d0xy2 = %f \n", d0xy1, d0xy2);
	    
	    hd0XYprod->Fill(d0xy1 * d0xy2, ptD);
	    hd0XY1->Fill(d0xy1, ptD);
	    hd0XY2->Fill(d0xy2, ptD);
	    arrsp[0] = invMassD;
	    arrsp[1] = ptD;
	    arrsp[2] = dist;
	    arrsp[3] = cosp;
	    arrsp[4] = TMath::Min(TMath::Abs(d0xy1),TMath::Abs(d0xy2));
	    arrsp[5] = d0xy1 * d0xy2;
	    arrsp[6] = dca;
	    arrsp[7] = TMath::Min(recProbe[0].GetTrack()->Pt(),recProbe[1].GetTrack()->Pt());
	    arrsp[8] = TMath::Abs(ipD);	    
	    arrsp[9] = cts;
	    hsp->Fill(arrsp);
	    
	    if (ntD0cand){
	      arrnt[0] = invMassD;
	      arrnt[1] = ptD;
	      arrnt[2] = dist;
	      arrnt[3] = cosp;
	      arrnt[4] = d0xy1;
	      arrnt[5] = d0xy2;
	      arrnt[6] = d0xy1 * d0xy2;
	      arrnt[7] = dca;
	      arrnt[8] = TMath::Min(recProbe[0].GetTrack()->Pt(),recProbe[1].GetTrack()->Pt());
	      arrnt[9] = TMath::Max(recProbe[0].GetTrack()->Pt(),recProbe[1].GetTrack()->Pt());
	      ntD0cand->Fill(arrnt);
	    }
	    
	  } // check on inv mass
	} // loop on mass hypothesis
      } // loop on first track
    } // loop on second track
    
    hcand->Fill(countCand);
    hcandpeak->Fill(countCandInPeak);
    printf(" --> Event %d, tot D0 candidates = %.0f  in peak = %.0f\n",iev,countCand,countCandInPeak);
  }
  
  fout->cd();
  hNevents->Write();
  hcand->Write();
  hcandpeak->Write();  
  hMassAll->Write();
  hYPtAll->Write();
  hPtAll->Write();
  hDistXY->Write();
  hDist->Write();
  hDCA->Write();
  hDCAx->Write();
  hDCAy->Write();
  hDCAz->Write();
  hCosp->Write();
  hCosThStVsMass->Write();
  hd0XYprod->Write();
  hd0XY1->Write();
  hd0XY2->Write();
  hsp->Write();
  fout->Close();
  if (ntD0cand){
    fnt->cd();
    ntD0cand->Write();
    fnt->Close();
  }
}




Double_t CosPointingAngle(Double_t vprim[3], Double_t vsec[3], TLorentzVector &parent)
{

    // /// XY
    // /// Cosine of pointing angle in transverse plane assuming it is produced
    // /// at "point"

    // TVector3 momXY(parent.Px(), parent.Py(), 0.);
    // TVector3 flineXY(vsec[0] - vprim[0],
    //                  vsec[1] - vprim[1],
    //                  0.);

    // Double_t ptot2 = momXY.Mag2() * flineXY.Mag2();
    // if (ptot2 <= 0)
    // {
    //     return 0.0;
    // }
    // else
    // {
    //     Double_t cos = momXY.Dot(flineXY) / TMath::Sqrt(ptot2);
    //     if (cos > 1.0)
    //         cos = 1.0;
    //     if (cos < -1.0)
    //         cos = -1.0;
    //     return cos;
    // }
    /// Cosine of pointing angle in space assuming it is produced at "point"

    TVector3 mom(parent.Px(), parent.Py(), parent.Pz());
    TVector3 fline(vsec[0] - vprim[0],
                   vsec[1] - vprim[1],
                   vsec[2] - vprim[2]);

    Double_t ptot2 = mom.Mag2() * fline.Mag2();
    if (ptot2 <= 0)
    {
        return 0.0;
    }
    else
    {
        Double_t cos = mom.Dot(fline) / TMath::Sqrt(ptot2);
        if (cos > 1.0)
            cos = 1.0;
        if (cos < -1.0)
            cos = -1.0;
        return cos;
    }
}

Double_t ImpParXY(Double_t vprim[3], Double_t vsec[3], TLorentzVector &parent){

  Double_t k = -(vsec[0]-vprim[0])*parent.Px()-(vsec[1]-vprim[1])*parent.Py();
  k /= parent.Pt()*parent.Pt();
  Double_t dx = vsec[0]-vprim[0]+k*parent.Px();
  Double_t dy = vsec[1]-vprim[1]+k*parent.Py();
  Double_t absImpPar = TMath::Sqrt(dx*dx+dy*dy);
  TVector3 mom(parent.Px(), parent.Py(), parent.Pz());
  TVector3 fline(vsec[0] - vprim[0],
		 vsec[1] - vprim[1],
		 vsec[2] - vprim[2]);
  TVector3 cross = mom.Cross(fline);
  return (cross.Z()>0. ? absImpPar : -absImpPar);
}

Double_t CosThetaStar(TLorentzVector &parent, TLorentzVector &dauk) {

  Double_t massMoth = TDatabasePDG::Instance()->GetParticle(421)->Mass();
  Double_t massK = TDatabasePDG::Instance()->GetParticle(321)->Mass();
  Double_t massPi = TDatabasePDG::Instance()->GetParticle(211)->Mass();

  Double_t pStar = TMath::Sqrt((massMoth*massMoth-massK*massK-massPi*massPi)*(massMoth*massMoth-massK*massK-massPi*massPi)-4.*massK*massK*massPi*massPi)/(2.*massMoth);

  Double_t pMoth=parent.P();
  Double_t e=TMath::Sqrt(massMoth*massMoth+pMoth*pMoth);
  Double_t beta = pMoth/e;
  Double_t gamma = e/massMoth;
  TVector3 momDau(dauk.Px(),dauk.Py(),dauk.Pz());
  TVector3 momMoth(parent.Px(),parent.Py(),parent.Pz());
  Double_t qlProng=momDau.Dot(momMoth)/momMoth.Mag();
  Double_t cts = (qlProng/gamma-beta*TMath::Sqrt(pStar*pStar+massK*massK))/pStar;

  return cts;
}

THnSparseF* CreateSparse(){
  const Int_t nAxes=10;
  TString axTit[nAxes]={"Inv. mass (GeV/c^{2})","p_{T} (GeV/c)",
			"Dec Len (cm)","cos(#vartheta_{p})",
			"d_0^{min} (cm)",
			"d_0*d_0 (cm^{2})","DCA",
			"p_{T}^{min} (GeV/c)",
			"d_0^{D} (cm)","cos(#theta*)"};
  Int_t bins[nAxes] =   {100,   5,  30,  20,   10,   10,      12,      8,  16,  10}; 
  Double_t min[nAxes] = {1.65,  0., 0., 0.98, 0.,   -0.0006, 0.0,   0.,  0.,  -1.};
  Double_t max[nAxes] = {2.15,  5., 0.3, 1.,   0.05, 0.,      0.03,  4.,  0.04, 1.};  
  THnSparseF *hsp = new THnSparseF("hsp", "hsp", nAxes, bins, min, max);
  for(Int_t iax=0; iax<nAxes; iax++) hsp->GetAxis(iax)->SetTitle(axTit[iax].Data());
  return hsp;
}


void ComputeVertex(KMCProbeFwd &t0, KMCProbeFwd &t1, Double_t &xV, Double_t &yV, Double_t &zV){
  // Based on AliESDv0

  //Trivial estimation of the vertex parameters
  Double_t tmp[3];
  t0.GetXYZ(tmp);
  Double_t  x1=tmp[0],  y1=tmp[1],  z1=tmp[2];
  const Double_t ss=0.0005*0.0005;//a kind of a residual misalignment precision
  Double_t sx1=t0.GetSigmaX2()+ss, sy1=t0.GetSigmaY2()+ss;
  t1.GetXYZ(tmp);
  Double_t  x2=tmp[0],  y2=tmp[1],  z2=tmp[2];
  Double_t sx2=t1.GetSigmaX2()+ss, sy2=t1.GetSigmaY2()+ss; 
    
  Double_t wx1=sx2/(sx1+sx2), wx2=1.- wx1;
  Double_t wy1=sy2/(sy1+sy2), wy2=1.- wy1;
  Double_t wz1=0.5, wz2=1.- wz1;
  xV=wx1*x1 + wx2*x2; 
  yV=wy1*y1 + wy2*y2; 
  zV=wz1*z1 + wz2*z2;
  return;
}
