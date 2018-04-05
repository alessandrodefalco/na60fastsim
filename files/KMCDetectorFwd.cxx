#include <TStopwatch.h>
#include <TRandom.h>
#include <TLorentzVector.h>
#include <TGenPhaseSpace.h>
#include "KMCDetectorFwd.h"
#include "AliMagF.h"
#include "AliLog.h"

ClassImp(KMCDetectorFwd)

const Double_t KMCDetectorFwd::kMassP  = 0.938;
const Double_t KMCDetectorFwd::kMassK  = 0.4937;
const Double_t KMCDetectorFwd::kMassPi = 0.1396;
const Double_t KMCDetectorFwd::kMassMu = 0.1057;
const Double_t KMCDetectorFwd::kMassE  = 0.0005;

const Double_t KMCDetectorFwd::fgkFldEps = 1.e-4;

KMCDetectorFwd::KMCDetectorFwd(const char *name, const char *title) 
:  TNamed(name,title)
  ,fNLayers(0)
  ,fNActiveLayers(0)
  ,fNActiveLayersITS(0)
  ,fNActiveLayersMS(0)
  ,fNActiveLayersTR(0)
  ,fLastActiveLayerITS(0)
  ,fLastActiveLayer(0)
  ,fLastActiveLayerTracked(0)
  ,fLayers()
  ,fBeamPipe(0)
  ,fVtx(0)
  ,fMaterials()
  ,fMagFieldID(kMagAlice)
  ,fProbe()
  ,fExternalInput(kFALSE)
  ,fIncludeVertex(kTRUE)
  ,fApplyBransonPCorrection(0.1)
  ,fUseBackground(kTRUE)
  ,fMaxChi2Cl(10.)
  ,fMaxNormChi2NDF(10)
  ,fMaxChi2Vtx(20)
  ,fMinITSHits(0)
  ,fMinMSHits(0)
  ,fMinTRHits(0)
  ,fMinP2Propagate(1.)
  ,fMaxChi2ClSQ(0)
  ,fMaxSeedToPropagate(0)
   //
,fDecayZProf(0)
,fZDecay(0)
,fDecMode(kNoDecay)
,fChi2MuVtx(0)
,fFldNReg(0)
,fFldZMins(0)
,fFldZMaxs(0)
,fDefStepAir(1.0)
,fDefStepMat(1.0)
,fImposeVertexPosition(kFALSE)
,fPattITS(0)
,fNCh(-1)
,fdNdY(0)
,fdNdPt(0)
,fHChi2Branson(0)
,fHChi2LrCorr(0)
,fHChi2NDFCorr(0)
,fHChi2NDFFake(0)
,fHChi2VtxCorr(0)
,fHChi2VtxFake(0)
,fHNCand(0)
,fHChi2MS(0)
,fHCandCorID(0)
,fZSteps(0)
{
  //
  // default constructor
  //
  //  fLayers = new TObjArray();
  fRefVtx[0] = fRefVtx[1] = fRefVtx[2] = 0;
}

KMCDetectorFwd::~KMCDetectorFwd() { // 
  // virtual destructor
  //
  //  delete fLayers;
  delete fdNdY;
  delete fdNdPt;
}


void KMCDetectorFwd::Print(const Option_t *opt) const
{
  // Prints the detector layout
  printf("KMCDetectorFwd %s: \"%s\"\n",GetName(),GetTitle());
  for (Int_t i = 0; i<fLayers.GetEntries(); i++) GetLayer(i)->Print(opt);
  if (fBeamPipe) fBeamPipe->Print(opt);
}

Double_t KMCDetectorFwd::ThetaMCS ( Double_t mass, Double_t radLength, Double_t momentum ) const
{
  //
  // returns the Multiple Couloumb scattering angle (compare PDG boolet, 2010, equ. 27.14)
  //
  Double_t beta  =  momentum / TMath::Sqrt(momentum*momentum+mass*mass)  ;
  Double_t theta =  0.0 ;    // Momentum and mass in GeV
  // if ( RadLength > 0 ) theta  =  0.0136 * TMath::Sqrt(RadLength) / ( beta * momentum );
  if ( radLength > 0 ) theta  =  0.0136 * TMath::Sqrt(radLength) / ( beta * momentum ) * (1+0.038*TMath::Log(radLength)) ;
  return (theta) ;
}

//__________________________________________________________________________
void KMCDetectorFwd::ReadMaterials(const char* fnam)
{
  // Read materials description from the external file
  //
  const char formMat[]="afffff?fff|";
  const char keyMat[]="material";
  const char modMat[]="";
  const char formMix[]="dafffff?fff|";
  const char keyMix[]="mixture";
  const char modMix[]="";
  //
  const Int_t kMinArgMat=6;
  const Int_t kMinArgMix=7;
  //
  NaCardsInput* inp;
  char *name;
  Float_t arg[3];
  Int_t narg;
  inp = new NaCardsInput();
  //
  if( !(inp->OpenFile(fnam)) ) {
    delete inp;
    Error("ReadMaterials","Did not find File %s",fnam);
    return;
  }
  //
  TObjArray* arr = &fMaterials;
  //
  // loop over all materials
  while( (narg = inp->FindEntry(keyMat,modMat,formMat,0,1))>-1 ) {
    name = inp->GetArg(0,"U"); // convert material name to upper case
    if (arr->FindObject(name)) {
      Warning("ReadMaterials","%s is already defined",name); continue;
    }
    Float_t* elbuf = 0;
    if ( narg>kMinArgMat ) {
      for (int i=0;i<3;i++) arg[i]=inp->GetArgF(kMinArgMat+i); // ELoss supplied
      elbuf = arg;
    }
    arr->AddLast(new NaMaterial(name,name,inp->GetArgF(1),inp->GetArgF(2),
				inp->GetArgF(3),inp->GetArgF(4),inp->GetArgF(5),elbuf));
  }
  //
  inp->Rewind(); // rewind the file
  // loop over all mixtures
  while( (narg = inp->FindEntry(keyMix,modMix,formMix,0,1))>-1 ) {
    name = inp->GetArg(1,"U"); // convert material name to upper case
    if ( arr->FindObject(name) ) {
      Warning("ReadMaterials","%s is already defined",name); continue;
    }
    Float_t* elbuf = 0;
    if ( narg>kMinArgMix ) {
      for (int i=0;i<3;i++) arg[i]=inp->GetArgF(kMinArgMix+i); // ELoss supplied
      elbuf = arg;
    }
    NaMixture* mix = new NaMixture(name,name,inp->GetArgF(2),inp->GetArgF(3),
				   inp->GetArgF(4),inp->GetArgF(5),
				   inp->GetArgF(6),elbuf);
    Int_t nmix = inp->GetArgD(0);
    Int_t nmixa = TMath::Abs(nmix);
    char* form = new char[nmixa+2]; // create format for mixture components
    for (int i=0;i<nmixa;i++) form[i]='f';
    form[nmixa] = '|'; form[nmixa+1]='\0';
    // Now read A,Z and W arrays
    // A array
    narg = inp->NextEntry();
    if ( inp->CompareArgList(form) ) {
      Error("ReadMaterials","mixture:A %d values are expected",nmixa);
      inp->Print(); delete[] form; continue;
    }
    Float_t* arrA = new Float_t[narg];
    for (int i=0;i<narg;i++) arrA[i] = inp->GetArgF(i);
    //
    // Z array
    narg = inp->NextEntry();
    if ( inp->CompareArgList(form) ) {
      Error("ReadMaterials","mixture:Z %d values are expected",nmixa);
      inp->Print(); delete[] form; delete[] arrA; continue;
    }
    Float_t* arrZ = new Float_t[narg];
    for (int i=0;i<narg;i++) arrZ[i] = inp->GetArgF(i);
    //
    // W array
    narg = inp->NextEntry();
    if ( inp->CompareArgList(form) ) {
      Error("ReadMaterials","mixture:W %d values are expected",nmixa);
      inp->Print(); delete[] form; delete[] arrA; delete[] arrZ; continue;
    }
    Float_t* arrW = new Float_t[narg];
    for (int i=0;i<narg;i++) arrW[i] = inp->GetArgF(i);
    //
    mix->SetComponents(nmix,arrA,arrZ,arrW);
    delete[] arrA; delete[] arrZ; delete[] arrW; delete[] form;
    arr->AddLast(mix);
    //
  }
  delete inp;
  //
}

//__________________________________________________________________________
void KMCDetectorFwd::ReadSetup(const char* setup, const char* materials)
{
  // load setup from external file
  ReadMaterials(materials);
  NaMaterial* mat = 0;
  int narg;
  //
  NaCardsInput *inp = new NaCardsInput();
  if( !(inp->OpenFile(setup)) ) {Error("BuilSetup","Did not find setup File %s",setup);exit(1);}
  //
  // flag for mag.field
  if ( (narg=inp->FindEntry("define","magfield","d|",1,1))>0 ) fMagFieldID = inp->GetArgD(0);
  printf("Magnetic Field: %s\n",fMagFieldID==kMagAlice ? "ALICE":Form("Custom%d",fMagFieldID));
  //
  // beampipe
  if ( (narg=inp->FindEntry("beampipe","","ffa|",1,1))<0 ) printf("No BeamPipe found in the setup %s\n",setup);
  else {
    mat = GetMaterial(inp->GetArg(2,"U"));
    if (!mat) {printf("Material %s is not defined\n",inp->GetArg(2,"U")); exit(1);}
    AddBeamPipe(inp->GetArgF(0), inp->GetArgF(1), mat->GetRadLength(), mat->GetDensity() );
  }
  //
  if ( (narg=inp->FindEntry("vertex","","ffff|",1,1))<0 ) printf("No vertex found in the setup %s\n",setup);
  else {
    fVtx = AddLayer("vtx","vertex",inp->GetArgF(0),0,0,inp->GetArgF(1), inp->GetArgF(2),inp->GetArgF(3));
  }
  //
  //
  // dummy material (not the official absorber
  inp->Rewind();
  while ( (narg=inp->FindEntry("dummy","","aaff|",0,1))>0 ) {
    mat = GetMaterial(inp->GetArg(1,"U"));
    if (!mat) {printf("Material %s is not defined\n",inp->GetArg(1,"U")); exit(1);}
    KMCLayerFwd* lr = AddLayer("dummy", inp->GetArg(0,"U"),  inp->GetArgF(2), mat->GetRadLength(), mat->GetDensity(), inp->GetArgF(3));
    lr->SetDead(kTRUE);
  }
  //
  // Absorber
  inp->Rewind();
  while ( (narg=inp->FindEntry("absorber","","aaff|",0,1))>0 ) {
    mat = GetMaterial(inp->GetArg(1,"U"));
    if (!mat) {printf("Material %s is not defined\n",inp->GetArg(1,"U")); exit(1);}
    KMCLayerFwd* lr = AddLayer("abs", inp->GetArg(0,"U"),  inp->GetArgF(2), mat->GetRadLength(), mat->GetDensity(), inp->GetArgF(3));
    lr->SetDead(kTRUE);
  }
  //
  // active layers
  //
  inp->Rewind();
  while ( (narg=inp->FindEntry("activelayer",0,"aaffff?fff",0,1))>0 ) {
    mat = GetMaterial(inp->GetArg(1,"U"));
    double eff = narg > 6 ? inp->GetArgF(6) : 1.0;
    double rmn = narg > 7 ? inp->GetArgF(7) : 0.0;
    double rmx = narg > 8 ? inp->GetArgF(8) : 1e9;
    if (!mat) {printf("Material %s is not defined\n",inp->GetArg(1,"U")); exit(1);}
    KMCLayerFwd* lr = AddLayer(inp->GetModifier(), inp->GetArg(0,"U"),  inp->GetArgF(2),  
			       mat->GetRadLength(), mat->GetDensity(), 
			       inp->GetArgF(3), inp->GetArgF(4), inp->GetArgF(5), eff);    
    lr->SetRMin(rmn);
    lr->SetRMax(rmx);
  }
  //-------------------------------------
  //
  // init mag field
  if (TGeoGlobalMagField::Instance()->GetField()) printf("Magnetic Field is already initialized\n");
  else {
    TVirtualMagField* fld = 0;
    if (fMagFieldID==kMagAlice) {
      fld = new AliMagF("map","map");
    }
    else { // custom field
      fld = new MagField(TMath::Abs(fMagFieldID));
    }
    TGeoGlobalMagField::Instance()->SetField( fld );
    TGeoGlobalMagField::Instance()->Lock();
  }
  //
  ClassifyLayers();
  //  BookControlHistos();
  //
}

//__________________________________________________________________________
KMCLayerFwd* KMCDetectorFwd::AddLayer(const char* type, const char *name, Float_t zPos, Float_t radL, Float_t density, 
				      Float_t thickness, Float_t xRes, Float_t yRes, Float_t eff) 
{
  //
  // Add additional layer to the list of layers (ordered by z position)
  // 
  KMCLayerFwd *newLayer = GetLayer(name);
  //
  if (!newLayer) {
    TString types = type;
    types.ToLower();
    newLayer = new KMCLayerFwd(name);
    newLayer->SetZ(zPos);
    newLayer->SetThickness(thickness);
    newLayer->SetX2X0( radL>0 ? thickness*density/radL : 0);
    newLayer->SetXTimesRho(thickness*density);
    newLayer->SetXRes(xRes);
    newLayer->SetYRes(yRes);
    newLayer->SetLayerEff(eff);
    if      (types=="vt")   newLayer->SetType(KMCLayerFwd::kITS);
    else if (types=="ms")   newLayer->SetType(KMCLayerFwd::kMS);
    else if (types=="tr")   newLayer->SetType(KMCLayerFwd::kTRIG);
    else if (types=="vtx")  {newLayer->SetType(KMCLayerFwd::kVTX); }
    else if (types=="abs")  {newLayer->SetType(KMCLayerFwd::kABS); newLayer->SetDead(kTRUE); }
    else if (types=="dummy")  {newLayer->SetType(KMCLayerFwd::kDUMMY); newLayer->SetDead(kTRUE); }
    //
    if (!newLayer->IsDead()) newLayer->SetDead( xRes==kVeryLarge && yRes==kVeryLarge);
    //
    if (fLayers.GetEntries()==0) fLayers.Add(newLayer);
    else {      
      for (Int_t i = 0; i<fLayers.GetEntries(); i++) {
	KMCLayerFwd *l = GetLayer(i);
	if (zPos<l->GetZ()) { fLayers.AddBefore(l,newLayer); break; }
	if (zPos>l->GetZ() && (i+1)==fLayers.GetEntries() ) { fLayers.Add(newLayer); } // even bigger then last one
      }      
    }
    //
  } else printf("Layer with the name %s does already exist\n",name);
  //
  return newLayer;
}

//______________________________________________________________________________________
void KMCDetectorFwd::AddBeamPipe(Float_t r, Float_t dr, Float_t radL, Float_t density) 
{
  //
  // mock-up cyl. beam pipe
  fBeamPipe = new BeamPipe((char*)"BeamPipe");
  fBeamPipe->SetRadius(r);
  fBeamPipe->SetThickness(dr);
  fBeamPipe->SetX2X0( radL>0 ? dr*density/radL : 0);
  fBeamPipe->SetXTimesRho(dr*density);  
  fBeamPipe->SetDead(kTRUE);
  //
}

//________________________________________________________________________________
void KMCDetectorFwd::ClassifyLayers()
{
  // assign active Id's, etc
  fLastActiveLayer = -1;
  fLastActiveLayerITS = -1;
  fNActiveLayers = 0;
  fNActiveLayersITS = 0;
  fNActiveLayersMS = 0;
  fNActiveLayersTR = 0;  
  //
  fNLayers = fLayers.GetEntries();
  for (int il=0;il<fNLayers;il++) {
    KMCLayerFwd* lr = GetLayer(il);
    lr->SetID(il);
    if (!lr->IsDead()) {
      fLastActiveLayer = il; 
      lr->SetActiveID(fNActiveLayers++);
      if (lr->IsITS()) {
	fLastActiveLayerITS = il;
	fNActiveLayersITS++;
	fLayersITS.AddLast(lr);
      }
      if (lr->IsMS())   {
	fNActiveLayersMS++;
	fLayersMS.AddLast(lr);
      }
      if (lr->IsTrig()) {
	fNActiveLayersTR++;
	fLayersTR.AddLast(lr);
      }
    }
  }
  //
  TVirtualMagField* fld = TGeoGlobalMagField::Instance()->GetField();
  if (fld->IsA() == MagField::Class()) {
    MagField* fldm = (MagField*) fld;
    fFldNReg = fldm->GetNReg();
    fFldZMins = fldm->GetZMin();
    fFldZMaxs = fldm->GetZMax();
    fDefStepAir = 100;
    fDefStepMat = 5;
  }
  else {
    fDefStepAir = 1;
    fDefStepMat = 1;
  }
  fZSteps = new Double_t[fFldNReg+2];
  //
  printf("DefStep: Air %f Mat: %f | N.MagField regions: %d\n",fDefStepAir,fDefStepMat,fFldNReg);
  //
  KMCProbeFwd::SetNITSLayers(fNActiveLayersITS + ((fVtx && !fVtx->IsDead()) ? 1:0));
  printf("KMCProbeFwd is initialized with %d slots\n",KMCProbeFwd::GetNITSLayers());
}

//_________________________________________________________________
KMCProbeFwd* KMCDetectorFwd::CreateProbe(double pt, double yrap, double phi, double mass, int charge, double x,double y, double z)
{
  // create track of given kinematics
  double xyz[3] = {x,y,z};
  double pxyz[3] = {pt*TMath::Cos(phi),pt*TMath::Sin(phi),TMath::Sqrt(pt*pt+mass*mass)*TMath::SinH(yrap)};
  KMCProbeFwd* probe = new KMCProbeFwd(xyz,pxyz,charge);
  probe->SetMass(mass);
  probe->SetTrID(-1);
  return probe;
}

//_________________________________________________________________
void KMCDetectorFwd::CreateProbe(KMCProbeFwd* adr, double pt, double yrap, double phi, double mass, int charge, double x,double y, double z)
{
  // create track of given kinematics
  double xyz[3] = {x,y,z};
  double pxyz[3] = {pt*TMath::Cos(phi),pt*TMath::Sin(phi),TMath::Sqrt(pt*pt+mass*mass)*TMath::SinH(yrap)};
  KMCProbeFwd* probe = new(adr) KMCProbeFwd(xyz,pxyz,charge);
  probe->SetTrID(-1);
  probe->SetMass(mass);
}

//_________________________________________________________________
KMCProbeFwd* KMCDetectorFwd::PrepareProbe(double pt, double yrap, double phi, double mass, int charge, double x,double y, double z)
{
  // Prepare trackable Kalman track at the farthest position
  //
  fGenPnt[0]=x;
  fGenPnt[1]=y;  
  fGenPnt[2]=z;  
  //
  if (fDecayZProf) { // decay is requested
    fZDecay  = fDecayZProf->GetRandom();
    fDecMode = kDoRealDecay;
    AliDebug(2,Form("Selected %.2f as decay Z",fZDecay));
  }
  // track parameters
  // Assume track started at (0,0,0) and shoots out on the X axis, and B field is on the Z axis
  fProbe.Reset();
  KMCProbeFwd* probe = CreateProbe(pt,yrap,phi,mass,charge,x,y,z);
  fProbe = *probe;     // store original track
  //
  // propagate to last layer
  fLastActiveLayerTracked = 0;
  int resp=0;
  KMCLayerFwd* lr=0,*lrP=0;
  for (Int_t j=0; j<=fLastActiveLayer; j++) {
    lrP = lr;
    lr = GetLayer(j);
    lr->Reset();
    if (!lrP) continue;
    //
    if (!(resp=PropagateToLayer(probe,lrP,lr,1))) return 0;
    KMCClusterFwd* cl = lr->GetCorCluster();
    double r = probe->GetR();
    //    printf("L%2d %f %f %f\n",j,r, lr->GetRMin(),lr->GetRMax());
    if (r<lr->GetRMax() && r>lr->GetRMin()) {
      if (resp>0) cl->Set(probe->GetXLoc(),probe->GetYLoc(), probe->GetZLoc(),probe->GetTrID());
      else cl->Kill();
    }
    else cl->Kill();
    //
    if (!lr->IsDead()) fLastActiveLayerTracked = j;
  }
  probe->ResetCovariance();// reset cov.matrix
  //  printf("Last active layer trracked: %d (out of %d)\n",fLastActiveLayerTracked,fLastActiveLayer);
  //
  return probe;
}

//____________________________________________________________________________
Int_t KMCDetectorFwd::GetFieldReg(double z) 
{
  // field region *2
  int ir = 0;
  for (int i=0;i<fFldNReg;i++) {
    if (z<=fFldZMins[i]) return ir;
    ir++;
    if (z<=fFldZMaxs[i]) return ir;
    ir++;
  }
  return ir;
}

//____________________________________________________________________________
Bool_t KMCDetectorFwd::PropagateToZBxByBz(KMCProbeFwd* trc,double z,double maxDZ,Double_t xOverX0,Double_t xTimesRho,Bool_t modeMC) 
{
  // propagate to given Z, checking for the field boundaries
  //
  double curZ = trc->GetZ();
  double dza = curZ-z;
  if (TMath::Abs(dza)<fgkFldEps) return kTRUE;

  // even id's correspond to Z between the field regions, odd ones - inside
  int ib0 = GetFieldReg(curZ); // field region id of start point
  int ib1 = GetFieldReg(z);    // field region id of last point
  int nzst = 0;
  //  AliInfo(Form("FldRegID: %d %d (%f : %f)",ib0,ib1, curZ,z));
  if (ib1>ib0) { // fwd propagation with field boundaries crossing
    for (int ib=ib0;ib<ib1;ib++) {
      if ( ib&0x1 ) { // we are in the odd (field ON) region, go till the end of field reg.
	//	printf("Here00 | %d %f\n",ib>>1,fFldZMaxs[ib>>1]);
	fZSteps[nzst++] = fFldZMaxs[ib>>1] + fgkFldEps;
      }
      else { // we are in even (field free) region, go till the beginning of next field reg.
	//	printf("Here01 | %d %f\n",ib>>1,fFldZMins[ib>>1]);
	fZSteps[nzst++] = fFldZMins[ib>>1] + fgkFldEps;
      }
    }
  }
  else if (ib1<ib0) { // bwd propagation
    for (int ib=ib0;ib>ib1;ib--) {
      if ( ib&0x1 ) { // we are in the odd (field ON) region, go till the beginning of field reg.
	//	printf("Here10 | %d %f\n",(ib-1)>>1,fFldZMins[(ib-1)>>1]);
	fZSteps[nzst++] = fFldZMins[(ib-1)>>1] - fgkFldEps;
      }
      else { // we are in even (field free) region, go till the beginning of next field reg.
	//	printf("Here11 | %d %f\n",(ib-1)>>1,fFldZMaxs[(ib-1)>>1]);
	fZSteps[nzst++] = fFldZMaxs[(ib-1)>>1] - fgkFldEps;
      }
    } 
  }
  fZSteps[nzst++] = z; // same field region, do nothing
  //
  //  printf("ZSteps: "); for (int ist=0;ist<nzst;ist++) printf("%+.5f ",fZSteps[ist]); printf("\n");

  for (int ist=0;ist<nzst;ist++) {
    double frc = (trc->GetZ()-fZSteps[ist])/dza;
    if (!trc->PropagateToZBxByBz(fZSteps[ist], maxDZ, frc*xOverX0, frc*xTimesRho, modeMC)) return kFALSE;
  }
  return kTRUE;
  //
}

//____________________________________________________________________________
Int_t KMCDetectorFwd::PropagateToLayer(KMCProbeFwd* trc, KMCLayerFwd* lrFrom, KMCLayerFwd* lrTo, int dir, Bool_t modeMC)
{
  // bring the track to lrTo, moving in direction dir (1: forward, -1: backward)
  // if relevant, account for the materials on lr
  //
  AliDebug(2,Form("From %d to %d, dir: %d",lrFrom? lrFrom->GetUniqueID():-1, lrTo->GetUniqueID(), dir));
  if (trc->GetTrack()->GetAlpha()<0) return -1;
  if ( lrFrom && dir*(lrTo->GetZ()-lrFrom->GetZ())<0 ) AliFatal(Form("Dir:%d Zstart: %f Zend: %f\n",dir,lrFrom->GetZ(),lrTo->GetZ()));
  //
  if      (dir>0 && (lrTo->GetZ()-0.5*lrTo->GetThickness()) < trc->GetZ() ) return -1;
  else if (dir<0 && (lrTo->GetZ()+0.5*lrTo->GetThickness()) > trc->GetZ() ) return -1;
  double dstZ;
  //
  if (lrFrom) {
    //
    if (!lrFrom->IsDead()) { // active layers are thin, no need for step by step tracking. The track is always in the middle
      AliDebug(2,Form("Correcting for mat.in active layer: X/X0: %f X*rho:%f ", lrFrom->GetX2X0(), dir*lrFrom->GetXTimesRho()));
      if (!trc->CorrectForMeanMaterial(lrFrom->GetX2X0(), -dir*lrFrom->GetXTimesRho(), modeMC)) return 0;
    }
    else {
      //
      dstZ = lrFrom->GetZ()+0.5*dir*lrFrom->GetThickness(); // go till the end of starting layer applying corrections
      if (dir==1 && trc->GetZ()<=fZDecay && dstZ>fZDecay) { // need to perform or to apply decay
	double frac = (fZDecay-trc->GetZ())/lrFrom->GetThickness();
	if (!PropagateToZBxByBz(trc,fZDecay, fDefStepMat, frac*lrFrom->GetX2X0(), -frac*lrFrom->GetXTimesRho(), modeMC)) return 0;
	PerformDecay(trc);
	frac = 1.-frac;
	if (!PropagateToZBxByBz(trc,dstZ, fDefStepMat, frac*lrFrom->GetX2X0(), -frac*lrFrom->GetXTimesRho(), modeMC)) return 0;
      }
      else if (!PropagateToZBxByBz(trc,dstZ, fDefStepMat, lrFrom->GetX2X0(), -dir*lrFrom->GetXTimesRho(), modeMC)) return 0;
    }
  }
  //
  dstZ = lrTo->GetZ();
  if (lrTo->IsDead()) dstZ += -dir*lrTo->GetThickness()/2; // for thick dead layers go till entrance
  //
  if (dir==1 && trc->GetZ()<=fZDecay && dstZ>fZDecay) { // need to perform or to apply decay
    if (!PropagateToZBxByBz(trc,fZDecay, fDefStepAir)) return 0;
    PerformDecay(trc);
  }
  if (!PropagateToZBxByBz(trc,dstZ, fDefStepAir)) return 0;
  //
  if (AliLog::GetGlobalDebugLevel()>=2) trc->GetTrack()->Print();

  return 1;
}

//________________________________________________________________________________
Bool_t KMCDetectorFwd::SolveSingleTrackViaKalman(double pt, double yrap, double phi, 
						 double mass, int charge, double x,double y, double z)
{
  // analytical estimate of tracking resolutions
  //  fProbe.SetUseLogTermMS(kTRUE);
  //
  for (int i=3;i--;) fGenPnt[i] = 0;
  if (fMinITSHits>fNActiveLayersITS) {
    fMinITSHits = fLastActiveLayerITS; 
    printf("Redefined request of min N ITS hits to %d\n",fMinITSHits);
  }
  //
  KMCProbeFwd* probe = PrepareProbe(pt,yrap,phi,mass,charge,x,y,z);
  if (!probe) return kFALSE;
  //
  KMCLayerFwd *lr = 0;
  //
  // Start the track fitting --------------------------------------------------------
  //
  // Back-propagate the covariance matrix along the track. 
  // Kalman loop over the layers
  //
  KMCProbeFwd* currTr = 0;
  lr = GetLayer(fLastActiveLayerTracked);
  lr->SetAnProbe(*probe);
  delete probe; // rethink...
  //
  int nupd = 0;
  for (Int_t j=fLastActiveLayerTracked; j--; ) {  // Layer loop
    //
    KMCLayerFwd *lrP = lr;
    lr = GetLayer(j);
    //
    lr->SetAnProbe( *lrP->GetAnProbe() );
    currTr = lr->GetAnProbe();
    currTr->ResetHit(lrP->GetActiveID());
    //
    // if there was a measurement on prev layer, update the track
    if (!lrP->IsDead()) { // include "ideal" measurement
      //      printf("Before update on %d : ",j); currTr->Print("etp");
      KMCClusterFwd* cl = lrP->GetCorCluster();
      if (!cl->IsKilled()) {
	if (!UpdateTrack(currTr,lrP,cl))  return kFALSE;
	nupd++;
      }
      //      printf("After update on %d (%+e %+e) : ",j, lrP->GetXRes(),lrP->GetYRes()); currTr->Print("etp");

    }

    if (!PropagateToLayer(currTr,lrP,lr,-1)) return kFALSE;      // propagate to current layer
    //
  } // end loop over layers
  // is MS reco ok?
  // check trigger
  int nhMS=0,nhTR=0,nhITS=0;
  for (int ilr=fNLayers;ilr--;) {
    KMCLayerFwd *lrt = GetLayer(ilr);
    if (lrt->IsTrig()) if (!lrt->GetCorCluster()->IsKilled()) nhTR++;
    if (lrt->IsMS())   if (!lrt->GetCorCluster()->IsKilled()) nhMS++;
    if (lrt->IsITS())  if (!lrt->GetCorCluster()->IsKilled()) nhITS++;
  }
  //  printf("ITS: %d MS: %d TR: %d\n",nhITS,nhMS,nhTR);
  if (nhTR<fMinTRHits) return kFALSE;
  if (nhMS<fMinMSHits) return kFALSE;
  //
  return kTRUE;
}

//____________________________________________________________________________
Bool_t KMCDetectorFwd::UpdateTrack(KMCProbeFwd* trc, const KMCLayerFwd* lr, const KMCClusterFwd* cl) const
{
  // update track with measured cluster
  // propagate to cluster
  if (cl->IsKilled()) return kTRUE;
  double meas[2] = {cl->GetY(),cl->GetZ()}; // ideal cluster coordinate, tracking (AliExtTrParam frame)
  double measErr2[3] = {lr->GetYRes()*lr->GetYRes(),0,lr->GetXRes()*lr->GetXRes()}; // !!! Ylab<->Ytracking, Xlab<->Ztracking
  //
  double chi2 = trc->GetTrack()->GetPredictedChi2(meas,measErr2);
  //    printf("Update for lr:%s -> chi2=%f\n",lr->GetName(), chi2);
  //    printf("cluster was :"); cl->Print("lc");
  //    printf("track   was :"); trc->Print("etp");  
    if (chi2>fMaxChi2Cl) return kTRUE; // chi2 is too large
    
  if (!trc->Update(meas,measErr2)) {
    AliDebug(2,Form("layer %s: Failed to update the track by measurement {%.3f,%3f} err {%.3e %.3e %.3e}",
		    lr->GetName(),meas[0],meas[1], measErr2[0],measErr2[1],measErr2[2]));
    if (AliLog::GetGlobalDebugLevel()>1) trc->Print("l");
    return kFALSE;
  }
  trc->AddHit(lr, chi2, cl->GetTrID());
  //
  return kTRUE;
}

//________________________________________________________________________________
Bool_t KMCDetectorFwd::SolveSingleTrack(double pt, double yrap, double phi, 
					double mass, int charge, double x,double y, double z, 
					TObjArray* sumArr,int nMC, int offset)
{
  // analityc and fullMC (nMC trials) evaluaion of tracks with given kinematics.
  // the results are filled in KMCTrackSummary objects provided via summArr array
  //
  //
  if (!SolveSingleTrackViaKalman(pt,yrap,phi,mass,charge,x,y,z)) return kFALSE;
  //
  /*
  int nsm = sumArr ? sumArr->GetEntriesFast() : 0;
  KMCLayerFwd* vtx = GetLayer(0);
  */
  //
  /*RS
  for (int i=0;i<nsm;i++) {
    KMCTrackSummary* tsm = (KMCTrackSummary*)sumArr->At(i);
    if (!tsm) continue;
    tsm->SetRefProbe( GetProbeTrack() ); // attach reference track (generated)
    tsm->SetAnProbe( vtx->GetAnProbe() ); // attach analitycal solution
  }
  */
  //
  if (offset<0) offset = fNLayers;

  TStopwatch sw;
  sw.Start();
  for (int it=0;it<nMC;it++) {
    //    printf("ev: %d\n",it);
    SolveSingleTrackViaKalmanMC(offset);
    /*RS
    KMCProbeFwd* trc = vtx->GetWinnerMCTrack();
    for (int ism=nsm;ism--;) { // account the track in each of summaries
      KMCTrackSummary* tsm = (KMCTrackSummary*)sumArr->At(ism);
      if (!tsm) continue;
      tsm->AddUpdCalls(GetUpdCalls());
      tsm->AddTrack(trc); 
    }
    */
  }
  //
  sw.Stop();
  //  printf("Total time: "); sw.Print();
  return kTRUE;
}

//____________________________________________________________________________
void KMCDetectorFwd::ResetMCTracks(Int_t maxLr)
{
  if (maxLr<0 || maxLr>=fNLayers) maxLr = fNLayers-1;
  for (int i=maxLr+1;i--;) GetLayer(i)->ResetMCTracks();
}

//____________________________________________________________
Bool_t KMCDetectorFwd::SolveSingleTrackViaKalmanMC(int offset)
{
  // MC estimate of tracking resolutions/effiencies. Requires that the SolveSingleTrackViaKalman
  // was called before, since it uses data filled by this method
  //
  // The MC tracking will be done starting from fLastActiveLayerITS + offset (before analytical estimate will be used)
  //
  // At this point, the fProbe contains the track params generated at vertex.
  // Clone it and propagate to target layer to generate hit positions affected by MS
  //
  const float kErrScale = 100.; // RS: this is the parameter defining the initial cov.matrix error wrt sensor resolution
      
  Bool_t checkMS = kTRUE;
  //
  KMCProbeFwd *currTrP=0,*currTr=0;
  static KMCProbeFwd trcConstr;
  int maxLr = fLastActiveLayerITS;
  if (offset>0) maxLr += offset;
  if (maxLr>fLastActiveLayer) maxLr = fLastActiveLayer;
  if (fExternalInput) maxLr = fLastActiveLayerTracked;
  if (fVtx && !fImposeVertexPosition) {
    fVtx->GetMCCluster()->Set(fProbe.GetTrack()->GetX(),fProbe.GetTrack()->GetY(),fProbe.GetTrack()->GetZ());
    fRefVtx[0] = fProbe.GetTrack()->GetY();
    fRefVtx[1] = fProbe.GetTrack()->GetZ();
    fRefVtx[2] = fProbe.GetTrack()->GetX();
  }
  //
  //printf("MaxLr: %d\n",maxLr);
  ResetMCTracks(maxLr);
  KMCLayerFwd* lr = GetLayer(maxLr);
  currTr = lr->AddMCTrack(&fProbe); // start with original track at vertex
  //
  //  printf("INI SEED: "); currTr->Print("etp");
  if (!fExternalInput) {if (!TransportKalmanTrackWithMS(currTr, maxLr, kFALSE)) return kFALSE;} // transport it to outermost layer where full MC is done
  else *currTr->GetTrack() = *GetLayer(maxLr)->GetAnProbe()->GetTrack();
  //printf("LastTrackedMS: "); currTr->GetTrack()->Print();
  //
  if (maxLr<=fLastActiveLayerTracked && maxLr>fLastActiveLayerITS) { // prolongation from MS
    // start from correct track propagated from above till maxLr
    double *covMS = (double*)currTr->GetTrack()->GetCovariance();
    const double *covIdeal = GetLayer(maxLr)->GetAnProbe()->GetCovariance();
    for (int i=15;i--;) covMS[i] = covIdeal[i];
  }
  else { // ITS SA: randomize the starting point
    currTr->ResetCovariance( kErrScale*TMath::Sqrt(lr->GetXRes()*lr->GetYRes()) ); // RS: this is the coeff to play with
  }
  //
  int fst = 0;
  const int fstLim = -1;

  for (Int_t j=maxLr; j--; ) {  // Layer loop
    //
    int ncnd=0,cndCorr=-1;
    KMCLayerFwd *lrP = lr;
    lr = GetLayer(j);
    int ntPrev = lrP->GetNMCTracks();
    //
    //    printf("Lr:%d %s IsDead:%d\n",j, lrP->GetName(),lrP->IsDead());
    if (lrP->IsDead()) { // for passive layer just propagate the copy of all tracks of prev layer >>>
      for (int itrP=ntPrev;itrP--;) { // loop over all tracks from previous layer
	currTrP = lrP->GetMCTrack(itrP); 
	if (currTrP->IsKilled()) continue;
	currTr = lr->AddMCTrack( currTrP );
	if (fst<fstLim) {
	  fst++;
	  currTr->Print("etp");
	}
	if (!PropagateToLayer(currTr,lrP,lr,-1))  {currTr->Kill(); lr->GetMCTracks()->RemoveLast(); continue;} // propagate to current layer
      }
      continue;
    } // treatment of dead layer <<<
    //
    if (lrP->IsMS() || lrP->IsTrig()) { // we don't consider bg hits in MS, just update with MC cluster
      KMCClusterFwd* clmc = lrP->GetMCCluster();
      for (int itrP=ntPrev;itrP--;) { // loop over all tracks from previous layer
	currTrP = lrP->GetMCTrack(itrP); if (currTrP->IsKilled()) continue;
	//	printf("At lr %d  | ",j+1); currTrP->Print("etp");
	currTr = lr->AddMCTrack( currTrP );
	if (fst<fstLim) {
	  fst++;
	  currTr->Print("etp");
	}
	// printf("\nAt Lr:%s ",lrP->GetName()); currTr->GetTrack()->Print();
	if (!clmc->IsKilled()) {
	  //	  lrP->Print("");
	  //	  printf("BeforeMS Update: "); currTr->Print("etp");
	  if (!UpdateTrack(currTr, lrP, clmc)) {currTr->Kill(); lr->GetMCTracks()->RemoveLast(); continue;} // update with correct MC cl.
	  //	  printf("AfterMS Update: "); currTr->Print("etp");
	}
	if (!PropagateToLayer(currTr,lrP,lr,-1))            {currTr->Kill(); lr->GetMCTracks()->RemoveLast(); continue;} // propagate to current layer	
      }      
      //      currTr->Print("etp");
      continue;
    } // treatment of ideal layer <<<
    //
    // active layer under eff. study (ITS?): propagate copy of every track to MC cluster frame (to have them all in the same frame)
    // and calculate the limits of bg generation
    //    KMCClusterFwd* clMC = lrP->GetMCCluster();
    //int nseeds = 0;
    // here ITS layers
    //   
    //
    AliDebug(2,Form("From Lr: %d | %d seeds, %d bg clusters",j+1,ntPrev,lrP->GetNBgClusters()));
    for (int itrP=0;itrP<ntPrev;itrP++) { // loop over all tracks from previous layer
      currTrP = lrP->GetMCTrack(itrP); if (currTrP->IsKilled()) continue;
      //
      if (checkMS) {
	checkMS = kFALSE;
	// check if muon track is well defined
	if (currTrP->GetNTRHits()<fMinTRHits) {currTrP->Kill(); continue;}
	if (currTrP->GetNMSHits()<fMinMSHits) {currTrP->Kill(); continue;}
	//
	//	printf("Check %d of %d at lr%d Nhits:%d NTR:%d NMS:%d\n",
	//       itrP,ntPrev,j, currTrP->GetNHits(),currTrP->GetNTRHits(),currTrP->GetNMSHits());
	if (fHChi2MS) fHChi2MS->Fill(currTr->GetChi2(),currTr->GetNHits());      
      }
      //
      // Are we entering to the last ITS layer? Apply Branson plane correction if requested
      if (lrP->GetID() == fLastActiveLayerITS && fVtx && !fVtx->IsDead() && fApplyBransonPCorrection>=0) {
	//	printf("%e -> %e (%d %d) | %e\n",lrP->GetZ(),lr->GetZ(), lrP->GetID(),j, currTr->GetZ());

	trcConstr = *currTrP;
	if (!PropagateToLayer(&trcConstr,lrP,fVtx,-1))  {currTrP->Kill();continue;} // propagate to vertex
	//////	trcConstr.ResetCovariance();
	// update with vertex point + eventual additional error
	float origErrX = fVtx->GetYRes(), origErrY = fVtx->GetXRes(); // !!! Ylab<->Ytracking, Xlab<->Ztracking
	KMCClusterFwd* clv = fVtx->GetMCCluster();
	double measCV[2] = {clv->GetY(),clv->GetZ()}, errCV[3] = {
	  origErrX*origErrX+fApplyBransonPCorrection*fApplyBransonPCorrection,
	  0.,
	  origErrY*origErrY+fApplyBransonPCorrection*fApplyBransonPCorrection
	};
	fChi2MuVtx = trcConstr.GetPredictedChi2(measCV,errCV);
	fHChi2Branson->Fill(fChi2MuVtx);
	//	printf("UpdVtx: {%+e %+e}/{%e %e %e}\n",measCV[0],measCV[1],errCV[0],errCV[1],errCV[2]);
	//	printf("Muon@Vtx:  "); trcConstr.Print("etp");
	if (!trcConstr.Update(measCV,errCV)) {currTrP->Kill();continue;}
	//	printf("Truth@VTX: "); fProbe.Print("etp");
	//	printf("Constraint@VTX: "); trcConstr.Print("etp");	
	if (!PropagateToLayer(&trcConstr,fVtx,lrP,1)) {currTrP->Kill();continue;}
	// constrain Muon Track
	//	printf("Constraint: "); trcConstr.Print("etp");

	//////	double measCM[2] = {trcConstr.GetYLoc(), trcConstr.GetZLoc()}, errCM[3] = {
	//////	  trcConstr.GetSigmaY2(),trcConstr.GetSigmaXY(),trcConstr.GetSigmaX2()
	//////	};

	//////	printf("UpdMuon: {%+e %+e}/{%e %e %e}\n",measCM[0],measCM[1],errCM[0],errCM[1],errCM[2]);
	//////	printf("Before constraint: "); currTrP->Print("etp");
	//////	if (!currTrP->Update(measCM,errCM)) {currTrP->Kill();continue;}
	(*currTrP->GetTrack()) = *trcConstr.GetTrack(); // override with constraint
	//	printf("After  constraint: "); currTrP->Print("etp");
	//	printf("MuTruth "); lrP->GetAnProbe()->Print("etp");
      }
      
      currTr = lr->AddMCTrack( currTrP );
      
      if (fst<fstLim) {
	fst++;
	currTr->Print("etp");
      }
      //
      AliDebug(2,Form("LastChecked before:%d",currTr->GetInnerLayerChecked()));
      CheckTrackProlongations(currTr, lrP,lr);
      AliDebug(2,Form("LastChecked after:%d",currTr->GetInnerLayerChecked()));
      ncnd++;
      if (currTr->GetNFakeITSHits()==0 && cndCorr<ncnd) cndCorr=ncnd;
      if (NeedToKill(currTr)) {currTr->Kill(); continue;}
    }
    /*
    if (ncnd>100) {
      printf("\n NCND= %d NPrev=  %d \n",ncnd,ntPrev);
      currTrP->Print("etp");
      Print("cl");
    }
    */
    if (fHNCand)     fHNCand->Fill(lrP->GetActiveID(), ncnd);
    if (fHCandCorID) fHCandCorID->Fill(lrP->GetActiveID(), cndCorr);
    //
    //  
    lr->GetMCTracks()->Sort();
    int ntTot = lr->GetNMCTracks(); // propagate max amount of allowed tracks to current layer
    if (ntTot>fMaxSeedToPropagate && fMaxSeedToPropagate>0) {
      for (int itr=ntTot;itr>=fMaxSeedToPropagate;itr--)  lr->GetMCTracks()->RemoveAt(itr);
      ntTot = fMaxSeedToPropagate;
    }
    //
    for (int itr=ntTot;itr--;) {
      currTr = lr->GetMCTrack(itr);
      if (!PropagateToLayer(currTr,lrP,lr,-1))  {currTr->Kill();continue;} // propagate to current layer
    }
    AliDebug(1,Form("Got %d tracks on layer %s",ntTot,lr->GetName()));
    //    lr->GetMCTracks()->Print();
    //
  } // end loop over layers    
  //
  // do we use vertex constraint?
  if (fVtx && !fVtx->IsDead() && fIncludeVertex) {
    printf("Apply vertex constraint\n");
    int ntr = fVtx->GetNMCTracks();
    for (int itr=0;itr<ntr;itr++) {
      currTr = fVtx->GetMCTrack(itr);
      if (currTr->IsKilled()) continue;
      double meas[2] = {0.,0.};
      if (fImposeVertexPosition) {
	meas[0] = fRefVtx[0];
	meas[1] = fRefVtx[1];	
      }
      else {
	KMCClusterFwd* clv = fVtx->GetMCCluster();
	meas[0] = clv->GetY();
	meas[1] = clv->GetZ();
      }
      double measErr2[3] = {fVtx->GetYRes()*fVtx->GetYRes(),0,fVtx->GetXRes()*fVtx->GetXRes()}; //  Ylab<->Ytracking, Xlab<->Ztracking
      double chi2v = currTr->GetPredictedChi2(meas,measErr2);
      if (!currTr->Update(meas,measErr2)) continue;
      //currTr->AddHit(fVtx->GetActiveID(), chi2v, -1);
      currTr->SetInnerLrChecked(fVtx->GetActiveID());
      //if (NeedToKill(currTr)) currTr->Kill();
      // if (fVtx->IsITS()) {if (!UpdateTrack(currTr, fVtx, fVtx->GetMCCluster(), kFALSE)) {currTr->Kill();continue;}}
    }
  }
  //  EliminateUnrelated(); //RSS
  int ntTot = lr->GetNMCTracks();
  ntTot = TMath::Min(1,ntTot);
  for (int itr=ntTot;itr--;) {
    currTr = lr->GetMCTrack(itr);
    if (currTr->IsKilled()) continue;
    if (fHChi2NDFCorr&&fHChi2NDFFake) {
      if (IsCorrect(currTr)) fHChi2NDFCorr->Fill(currTr->GetNITSHits(),currTr->GetNormChi2(kTRUE));
      else                   fHChi2NDFFake->Fill(currTr->GetNITSHits(),currTr->GetNormChi2(kTRUE));
    }
  }
  //  
  return kTRUE;
}

//________________________________________________________________________________
Bool_t KMCDetectorFwd::TransportKalmanTrackWithMS(KMCProbeFwd *probTr, int maxLr, Bool_t bg)
{
  // Transport track till layer maxLr, applying random MS
  //
  int resP = 0;
  for (Int_t j=0; j<maxLr; j++) {
    KMCLayerFwd* lr0 = GetLayer(j);
    KMCLayerFwd* lr  = GetLayer(j+1);
    //
    if (!(resP=PropagateToLayer(probTr,lr0,lr, 1, kTRUE))) return kFALSE;
    if (lr->IsDead()) continue;
    if (resP<0) {lr->GetMCCluster()->Kill(); continue;}
    double r = probTr->GetR();
    if (r>lr->GetRMax() || r<lr->GetRMin()) {
      /*if (!bg)*/ lr->GetMCCluster()->Kill(); 
      continue;
    }
    // store randomized cluster local coordinates and phi
    double rx,ry;
    gRandom->Rannor(rx,ry);
    /*
    double clxyz[3],clxyzL[3];
    probTr->GetXYZ(clxyz);
    clxyz[0] += rx*lr->GetXRes();
    clxyz[1] += ry*lr->GetYRes();
    KMCProbeFwd::Trk2Lab(clxyz, clxyzL);
    lr->GetMCCluster()->Set(clxyzL[0],clxyzL[1],clxyzL[2]);
    */
    // cluster is stored in local frame
    if (bg) {
      lr->AddBgCluster(probTr->GetXLoc(), probTr->GetYLoc()+ry*lr->GetYRes(), probTr->GetZLoc()-rx*lr->GetXRes(),probTr->GetTrID());
    }
    else lr->GetMCCluster()->Set(probTr->GetXLoc(), probTr->GetYLoc()+ry*lr->GetYRes(), probTr->GetZLoc()-rx*lr->GetXRes(),probTr->GetTrID());
    //
  }
  //
  return kTRUE;
}

//____________________________________________________________________________
void KMCDetectorFwd::CheckTrackProlongations(KMCProbeFwd *probe, KMCLayerFwd* lrP, KMCLayerFwd* lr)
{
  // explore prolongation of probe from lrP to lr with all possible clusters of lrP
  // the probe is already brought to clusters frame
  // for the last ITS plane apply Branson correction
  //if (lrP->GetUniqueID()==fLastActiveLayerITS) probe->Print("etp");
  /*
  if (lrP->GetUniqueID()==fLastActiveLayerITS && fVtx) {
    printf("Before Branson: "); probe->Print("etp");
    double zP = probe->GetZ();
    if (!PropagateToZBxByBz(probe,fVtx->GetZ(),fDefStepAir)) return;
    double measVErr2[3] = {fVtx->GetYRes()*fVtx->GetYRes(),0,fVtx->GetXRes()*fVtx->GetXRes()}; // we work in tracking frame here!
    double measV[2] = {0,0};
    probe->Update(measV,measVErr2);
    if (!PropagateToZBxByBz(probe,zP,fDefStepAir)) return;
    printf("After Branson: "); probe->Print("etp");
  }
  */
  static KMCProbeFwd propVtx;
  //
  int nCl = lrP->GetNBgClusters();
  double measErr2[3] = {lrP->GetYRes()*lrP->GetYRes(),0,lrP->GetXRes()*lrP->GetXRes()}; // we work in tracking frame here!
  double meas[2] = {0,0};
  double tolerY = probe->GetTrack()->GetSigmaY2() + measErr2[0];
  double tolerZ = probe->GetTrack()->GetSigmaZ2() + measErr2[2];
  tolerY = TMath::Sqrt(fMaxChi2Cl*tolerY);
  tolerZ = TMath::Sqrt(fMaxChi2Cl*tolerZ);
  double yMin = probe->GetTrack()->GetY() - tolerY;
  double yMax = yMin + tolerY+tolerY;    
  double zMin = probe->GetTrack()->GetZ() - tolerZ;
  double zMax = zMin + tolerZ + tolerZ;
  //probe->GetTrack()->Print();
  //
  probe->SetInnerLrChecked(lrP->GetActiveID());
  AliDebug(2,Form("From Lr(%d) %s to Lr(%d) %s | LastChecked %d",
		  lrP->GetActiveID(),lrP->GetName(),lr->GetActiveID(),lr->GetName(),probe->GetInnerLayerChecked()));
  for (int icl=-1;icl<nCl;icl++) {
    //
    if (gRandom->Rndm() > lrP->GetLayerEff()) continue; // generate layer eff
    //
    KMCClusterFwd *cl = icl<0 ? lrP->GetMCCluster() : lrP->GetBgCluster(icl);  // -1 is for true MC cluster
    if (cl->IsKilled()) {
      if (AliLog::GetGlobalDebugLevel()>1) {printf("Skip cluster %d ",icl); cl->Print();}
      continue;
    }
    double y = cl->GetY(); // ! tracking frame coordinates: Ylab
    double z = cl->GetZ(); //                              -XLab, sorted in decreasing order
   //
    AliDebug(2,Form("Check against cl#%d(%d) out of %d at layer %s | y: Tr:%+8.4f Cl:%+8.4f (%+8.4f:%+8.4f) z: Tr:%+8.4f Cl: %+8.4f (%+8.4f:%+8.4f)",
		    icl,cl->GetTrID(),nCl,lrP->GetName(), probe->GetTrack()->GetY(),y,yMin,yMax,probe->GetTrack()->GetZ(),z,zMin,zMax));
    //
    if (z>zMax) {if (icl==-1) continue; else break;} // all other z will be even smaller, no chance to match
    if (z<zMin) continue;
    if (y<yMin || y>yMax) continue; 
    //
    meas[0] = y; meas[1] = z;
    double chi2 = probe->GetPredictedChi2(meas,measErr2);
    //
    //    AliInfo(Form("Seed-to-cluster chi2 = Chi2=%.2f for cl:",chi2));
    //      cl->Print("lc");
    //    AliDebug(2,Form("Seed-to-cluster chi2 = Chi2=%.2f",chi2));
    if (icl<0 && fHChi2LrCorr) fHChi2LrCorr->Fill(lrP->GetActiveID(), chi2);
    if (chi2>fMaxChi2Cl) continue;
    // 
    //    printf("Lr%d | cl%d, chi:%.3f X:%+.4f Y:%+.4f | x:%+.4f y:%+.4f |Sg: %.4f %.4f\n",
    //	   lrP->GetActiveID(),icl,chi2, (zMin+zMax)/2,(yMin+yMax)/2, z,y, tolerZ/TMath::Sqrt(fMaxChi2Cl),tolerY/TMath::Sqrt(fMaxChi2Cl));
    // update track copy
    KMCProbeFwd* newTr = lr->AddMCTrack( probe );
    if (!newTr->Update(meas,measErr2)) {
      AliDebug(2,Form("Layer %s: Failed to update the track by measurement {%.3f,%3f} err {%.3e %.3e %.3e}",
		      lrP->GetName(),meas[0],meas[1], measErr2[0],measErr2[1],measErr2[2]));
      if (AliLog::GetGlobalDebugLevel()>1) newTr->Print("l");
      newTr->Kill();
      lr->GetMCTracks()->RemoveLast();
      continue;
    }
    if (fMinP2Propagate>0) {
      double p = newTr->GetTrack()->GetP();
      if (p<fMinP2Propagate) {
	newTr->Kill();
	lr->GetMCTracks()->RemoveLast();
	continue;
      }
    }
    newTr->AddHit(lrP, chi2, cl->GetTrID());

    //////////////////// check chi2 to vertex
    if (fVtx && !fVtx->IsDead() && fMaxChi2Vtx>0) {
      double measVErr2[3] = {fVtx->GetYRes()*fVtx->GetYRes(),0,fVtx->GetXRes()*fVtx->GetXRes()}; // we work in tracking frame here!
      propVtx = *newTr;

      if (!PropagateToZBxByBz(&propVtx,fRefVtx[2],fDefStepAir)) {/*printf("???????? Kill\n");*/ newTr->Kill(); lr->GetMCTracks()->RemoveLast();}
      double chi2V = propVtx.GetTrack()->GetPredictedChi2(fRefVtx,measVErr2);
      if (fHChi2VtxCorr && fHChi2VtxFake) {
	if (IsCorrect(newTr)) fHChi2VtxCorr->Fill(newTr->GetNITSHits(),chi2V);
	else                  fHChi2VtxFake->Fill(newTr->GetNITSHits(),chi2V);
      }
      AliDebug(2,Form("Chi2 to vertex: %f | y: Tr:%+8.4f Cl:%+8.4f  z: Tr:%+8.4f Cl: %+8.4f",chi2V,
		      propVtx.GetTrack()->GetY(),fRefVtx[0],
		      propVtx.GetTrack()->GetZ(),fRefVtx[1]));

      if (chi2V>fMaxChi2Vtx) {
	newTr->Kill();
	lr->GetMCTracks()->RemoveLast();
	continue;
      }

      /*
      double pz = 1./TMath::Abs(propVtx.GetTrack()->Get1P());
      if (pz<1) {
	printf("LowMom: %f | chi2V=%f (%f | %f %f %f)  ",pz,chi2V, fMaxChi2Vtx, fRefVtx[0],fRefVtx[1],fRefVtx[2]);  propVtx.Print("etp");       
      }
      */

    }

    ////////////////////////////////////////

    //    if (!PropagateToLayer(newTr,lrP,lr,-1)) {newTr->Kill(); continue;} // propagate to next layer
    if (AliLog::GetGlobalDebugLevel()>1) {
      AliInfo("Cloned updated track is:");
      newTr->Print();
    }
  }
  //
}

//_________________________________________________________
//Double_t KMCDetectorFwd::HitDensity(double xLab,double ylab,double zlab ) const
Double_t KMCDetectorFwd::HitDensity(double ,double ,double  ) const
{
  // RS to do
  return 1;
}

//_________________________________________________________
Bool_t KMCDetectorFwd::NeedToKill(KMCProbeFwd* probe) const
{
  // check if the seed should be killed
  const Bool_t kModeKillMiss = kFALSE;
  Bool_t kill = kFALSE;
  while (1) {
    int il = probe->GetInnerLayerChecked();
    int nITS = probe->GetNITSHits();
    int nITSMax = nITS + il; // maximum it can have
    if (nITSMax<fMinITSHits) {
      kill = kTRUE; 
      break;
    }    // has no chance to collect enough ITS hits
    //
    int ngr = fPattITS.GetSize();
    if (ngr>0) { // check pattern
      UInt_t patt = probe->GetHitsPatt();
      // complete the layers not checked yet
      for (int i=il;i--;) patt |= (0x1<<i);
      for (int ig=ngr;ig--;) 
	if (!(((UInt_t)fPattITS[ig]) & patt)) {
	  kill = kTRUE; 
	  break;
	}
      //
    }
    //
    if (nITS>2) {  // check if smallest possible norm chi2/ndf is acceptable
      double chi2min = probe->GetChi2ITS();
      if (kModeKillMiss) {
	int nMiss = fNActiveLayersITS - probe->GetInnerLayerChecked() - nITS; // layers already missed
	chi2min = nMiss*probe->GetMissingHitPenalty();
      }
      chi2min /= ((nITSMax<<1)-KMCProbeFwd::kNDOF);
      if (chi2min>fMaxNormChi2NDF) {
	kill = kTRUE; 
	break;
      }
    }
    //
    /*
    // loose vertex constraint
    double dst;
    if (nITS>=2) {
      probe->GetZAt(0,fBFieldG,dst);
      //printf("Zd (F%d): %f\n",probe->GetNFakeITSHits(),dst);
      if (TMath::Abs(dst)>10.) {
	kill = kTRUE; 
	break;
      }
    }
    if (nITS>=3) {
      probe->GetYAt(0,fBFieldG,dst);
      //printf("Dd (F%d): %f\n",probe->GetNFakeITSHits(),dst);
      if (TMath::Abs(dst)>10.) {
	kill = kTRUE; 
	break;
      }
    }
    */
    //
    break;
  }
  if (kill && AliLog::GetGlobalDebugLevel()>1 && probe->GetNFakeITSHits()==0) {
    printf("Killing good seed, last upd layer was %d\n",probe->GetInnerLayerChecked());
    probe->Print("l");
  }
  return kill;  
  //
}

//_________________________________________________________
void KMCDetectorFwd::PerformDecay(KMCProbeFwd* trc)
{
  // Decay track
  if (fDecMode==kNoDecay) return;
  //  printf("DecMode: %d\n",fDecMode);
  static TGenPhaseSpace decay;
  static TLorentzVector pDecParent,pDecMu,pParCM;
  //
  const double kTol = 5e-3; // 5 MeV tolerance
  double mass = trc->GetMass();  
  double pxyz[3],xyz[3]={0};
  trc->GetPXYZ(pxyz);
  AliDebug(2,Form(" >>Mode:%d at Z=%f, PXYZ:%+6.3f %+6.3f %+6.3f, Mass:%.3f",fDecMode,fZDecay,pxyz[0],pxyz[1],pxyz[2],mass));
  static double ctau = 1e10;
  if (fDecMode==kDoRealDecay) {
    //
    if (TMath::Abs(mass-kMassMu)<kTol) {AliInfo(Form("Decay requested but provided mass %.4f hints to muon",mass)); exit(1);}
    if (TMath::Abs(mass-kMassPi)<kTol) {
      mass = kMassPi;
      Double_t masses[2] = {kMassMu, kMassE};
      pParCM.SetXYZM(0,0,0,mass);
      decay.SetDecay(pParCM, 2, masses);
      ctau = 780.45;
    }
    else if (TMath::Abs(mass-kMassK)<kTol) {
      mass = kMassK;
      Double_t masses[3] = {kMassMu, 0};
      pParCM.SetXYZM(0,0,0,mass);
      decay.SetDecay(pParCM, 2, masses);
      ctau = 371.2;
    }
    else {AliInfo(Form("Decay requested but provided mass %.4f is not recognized as pi or K",mass)); exit(1);}
    //
    decay.Generate();
    pDecMu = *decay.GetDecay(0); // muon kinematics in parent frame
    fDecMode = kApplyDecay;
  }
  //
  pDecParent.SetXYZM(pxyz[0],pxyz[1],pxyz[2],pParCM.M());
  pDecMu = *decay.GetDecay(0);
  pDecMu.Boost(pDecParent.BoostVector());
  //
  pxyz[0] = pDecMu.Px();
  pxyz[1] = pDecMu.Py();
  pxyz[2] = pDecMu.Pz();
  //
  static KMCProbeFwd tmpPr;
  tmpPr.Init(xyz,pxyz,trc->GetTrack()->Charge());
  double* parTr  = (double*)trc->GetTrack()->GetParameter();
  double* parNew = (double*)tmpPr.GetTrack()->GetParameter();  
  for (int i=0;i<5;i++) parTr[i] = parNew[i];
  trc->SetMass(kMassMu);
  // 
  // set decay weight
  trc->GetXYZ(xyz);
  for (int i=3;i--;) xyz[i]-=fGenPnt[i];
  double dst = TMath::Sqrt(xyz[0]*xyz[0]+xyz[1]*xyz[1]+xyz[2]*xyz[2]);
  double ctgamma = ctau*pDecParent.Gamma();
  double exparg = dst/ctgamma;
  //  double wgh =  exparg<100 ? 1.-TMath::Exp(-exparg) : 1.0;
  double wgh =  exparg<100 ? TMath::Exp(-exparg) : 0;
  // account for the losses due to the hadron interactions >>>
  double wabs = 0;
  int nb = fDecayZProf->GetNbinsX();
  for (int ib=1;ib<nb;ib++) {
    double x = fDecayZProf->GetBinCenter(ib);
    exparg = x/ctgamma;
    if (exparg>100) break;
    double wb = fDecayZProf->GetBinContent(ib);
    wabs += TMath::Exp(-exparg)*wb*fDecayZProf->GetBinWidth(ib);
    if (wb<1e-9) break;
  }
  wgh *= wabs/ctgamma;
  // account for the losses due to the hadron interactions <<<
  trc->SetWeight(wgh);
  //  printf("Decay %.3f Z:%+7.3f ctau: %f gamma %f wgh:%e\n",mass,fZDecay,ctau, pDecMu.Gamma(),wgh);
  //
  AliDebug(2,Form(" <<Mode:%d at Z=%f, PXYZ:%+6.3f %+6.3f %+6.3f, Mass:%.3f, Wgh:%.3e",fDecMode,fZDecay,pxyz[0],pxyz[1],pxyz[2],kMassMu,wgh));
  //AliInfo(Form(" <<Mode:%d at Z=%f, PXYZ:%+6.3f %+6.3f %+6.3f, Mass:%.3f, Wgh:%.3e",fDecMode,fZDecay,pxyz[0],pxyz[1],pxyz[2],kMassMu,wgh));
  //
}

//_________________________________________________________
void KMCDetectorFwd::InitDecayZHisto(double absorberLambda)
{
  // prepare a profile of Zdecay: uniform till start of the absorber, then exponentially dumped
  if (absorberLambda<1) absorberLambda = 1;
  KMCLayerFwd* labs = 0;
  for (int i=0;i<fNLayers;i++) {
    if ( (labs=GetLayer(i))->IsAbs()) break;
    labs = 0;
  }
  if (!labs) {
    AliError("Could not find beginning of the absorber, is setup loaded?");
    exit(1);
  }
  double zdmp = labs->GetZ()-labs->GetThickness()/2;
  double zmax = GetLayer(fLastActiveLayer)->GetZ();
  AliInfo(Form("Decay will be done uniformly till Z=%.1f, then dumped with Lambda=%.1f cm",zdmp,absorberLambda));
  //
  int nbn = int(zmax-zdmp+1);
  TH1F* hd = new TH1F("DecayZProf","Z decay profile",nbn,0,zmax);
  for (int i=1;i<=nbn;i++) {
    double z = hd->GetBinCenter(i);
    if (z<zdmp) hd->SetBinContent(i,1.);
    else {
      double arg = (z-zdmp)/absorberLambda;
      hd->SetBinContent(i, arg>100 ? 0 : TMath::Exp(-arg));
    }
  }
  SetDecayZProfile(hd);
  //
}

//_________________________________________________________
void KMCDetectorFwd::GenBgEvent(double x, double y, double z, int offset)
{
  if (fNChPi<0 && fNChK<0 && fNChP<0) return;
  // generate bg. events from simple thermalPt-gaussian Y parameterization
  if (!fdNdYPi || !fdNdYK || !fdNdYP || !fdNdPtPi || !fdNdPtK || !fdNdPtP) AliFatal("Background generation was not initialized");
  //
  int maxLr = fLastActiveLayerITS + offset;
  if (maxLr > fLastActiveLayer) maxLr = fLastActiveLayer;
  //
  for (int ilr=fLastActiveLayer;ilr--;) {
    KMCLayerFwd* lr = GetLayer(ilr);
    if (lr->IsDead()) continue;
    lr->ResetBgClusters();
  }
  int decMode = fDecMode;
  fDecMode = kNoDecay;
  KMCProbeFwd bgtr;
  //
  //  double ntr = gRandom->Poisson( fNCh );
//   for (int itr=0;itr<ntr;itr++) {
//     double yrap  = fdNdY->GetRandom();
//     double pt = fdNdPt->GetRandom();
//     double phi = gRandom->Rndm()*TMath::Pi()*2;
//     int charge = gRandom->Rndm()>0.5 ? 1:-1;
//     CreateProbe(&bgtr, pt, yrap, phi, kMassPi, charge, x,y,z);
//     bgtr.SetTrID(itr);
//     TransportKalmanTrackWithMS(&bgtr, maxLr,kTRUE);
//   }
  // pions
  double ntrPi = gRandom->Poisson( fNChPi );
  printf("fNChPi=%f ntrPi=%f\n",fNChPi,ntrPi);
  for (int itr=0;itr<ntrPi;itr++) {
    double yrap  = fdNdYPi->GetRandom();
    double pt = fdNdPtPi->GetRandom();
    double phi = gRandom->Rndm()*TMath::Pi()*2;
    int charge = gRandom->Rndm()>0.52 ? 1:-1;
    CreateProbe(&bgtr, pt, yrap, phi, kMassPi, charge, x,y,z);
    bgtr.SetTrID(itr);
    TransportKalmanTrackWithMS(&bgtr, maxLr,kTRUE);
  }
  // kaons
  double ntrK = gRandom->Poisson( fNChK );
  printf("fNChK=%f ntrK=%f\n",fNChK,ntrK);
  for (int itr=0;itr<ntrK;itr++) {
    double yrap  = fdNdYK->GetRandom();
    double pt = fdNdPtK->GetRandom();
    double phi = gRandom->Rndm()*TMath::Pi()*2;
    int charge = gRandom->Rndm()>0.3 ? 1:-1;
    CreateProbe(&bgtr, pt, yrap, phi, kMassK, charge, x,y,z);
    bgtr.SetTrID(itr+ntrPi);
    TransportKalmanTrackWithMS(&bgtr, maxLr,kTRUE);
  }
  // protons
  double ntrP = gRandom->Poisson( fNChP );
  printf("fNChP=%f ntrP=%f\n",fNChP,ntrP);
  for (int itr=0;itr<ntrP;itr++) {
    double yrap  = fdNdYP->GetRandom();
    double pt = fdNdPtP->GetRandom();
    double phi = gRandom->Rndm()*TMath::Pi()*2;
    int charge = 1;
    CreateProbe(&bgtr, pt, yrap, phi, kMassP, charge, x,y,z);
    bgtr.SetTrID(itr+ntrPi+ntrK);
    TransportKalmanTrackWithMS(&bgtr, maxLr,kTRUE);
  }
  //
  for (int ilr=maxLr;ilr--;) {
    KMCLayerFwd* lr = GetLayer(ilr);
    if (lr->IsDead()) continue;
    lr->SortBGClusters();
  }
  fDecMode = decMode;
  //  
}

//_________________________________________________________
void KMCDetectorFwd::InitBgGeneration(int dndeta, 
				      double y0, double sigy, double ymin,double ymax,
				      double T, double ptmin, double ptmax)
{
  // initialize bg generation routines
  fNCh = dndeta*0.5*(TMath::Erf((ymax-y0)/sqrt(2.)/sigy)-TMath::Erf((ymin-y0)/sqrt(2.)/sigy));
  fdNdY = new TF1("dndy","exp( -0.5*pow( (x-[0])/[1],2) )",ymin,ymax);
  fdNdY->SetParameters(y0,sigy);
  //
  fdNdPt = new TF1("dndpt","x*exp(-sqrt(x*x+1.949e-02)/[0])",ptmin,ptmax); // assume pion
  fdNdPt->SetParameter(0,T);
}

//_________________________________________________________
void KMCDetectorFwd::InitBgGenerationPart(double NPi,double NKplus,double NKminus ,double NP,double Piratio,
					  double y0, double y0Pi,double y0Kplus,double y0Kminus,double y0P,
					  double sigyPi, double sigyKplus,double sigyKminus, double sigyP,
					  double ymin,double ymax,
					  double Tpi, double TK, double TP, double ptmin, double ptmax)
{
  // initialize bg generation routines
  fNChPi = NPi*(1+Piratio)*sigyPi*TMath::Sqrt(TMath::Pi()/2.)*(TMath::Erf((ymax-y0-y0Pi)/sqrt(2.)/sigyPi)+ TMath::Erf((ymax-y0+y0Pi)/sqrt(2.)/sigyPi)-TMath::Erf((ymin-y0-y0Pi)/sqrt(2.)/sigyPi)-TMath::Erf((ymin-y0+y0Pi)/sqrt(2.)/sigyPi));
  fNChK = NKplus*sigyKplus*TMath::Sqrt(TMath::Pi()/2.)*(TMath::Erf((ymax-y0-y0Kplus)/sqrt(2.)/sigyKplus)+ TMath::Erf((ymax-y0+y0Kplus)/sqrt(2.)/sigyKplus)-TMath::Erf((ymin-y0-y0Kplus)/sqrt(2.)/sigyKplus)-TMath::Erf((ymin-y0+y0Kplus)/sqrt(2.)/sigyKplus))+
    NKminus*sigyKminus*TMath::Sqrt(TMath::Pi()/2.)*(TMath::Erf((ymax-y0-y0Kminus)/sqrt(2.)/sigyKminus)+ TMath::Erf((ymax-y0+y0Kminus)/sqrt(2.)/sigyKminus)-TMath::Erf((ymin-y0-y0Kminus)/sqrt(2.)/sigyKminus)-TMath::Erf((ymin-y0+y0Kminus)/sqrt(2.)/sigyKminus));
  fNChP = NP*sigyP*TMath::Sqrt(TMath::Pi()/2.)*(TMath::Erf((ymax-y0-y0P)/sqrt(2.)/sigyP)+ TMath::Erf((ymax-y0+y0P)/sqrt(2.)/sigyP)-TMath::Erf((ymin-y0-y0P)/sqrt(2.)/sigyP)-TMath::Erf((ymin-y0+y0P)/sqrt(2.)/sigyP));
  //
  fdNdYPi = new TF1("dndy","exp( -0.5*pow( (x-[0]-[1])/[2],2) )+ exp( -0.5*pow( (x-[0]+[1])/[2],2) )",ymin,ymax);
  fdNdYPi->SetParameters(y0,y0Pi,sigyPi);
  fdNdYK  = new TF1("dndy","exp( -0.5*pow( (x-[0]-[1])/[2],2) )+ exp( -0.5*pow( (x-[0]+[1])/[2],2) )",ymin,ymax);
  fdNdYK ->SetParameters(y0,y0Kplus ,sigyKplus);
  fdNdYP  = new TF1("dndy","exp( -0.5*pow( (x-[0]-[1])/[2],2) )+ exp( -0.5*pow( (x-[0]+[1])/[2],2) )",ymin,ymax);
  fdNdYP ->SetParameters(y0,y0P,sigyP);
  //
  fdNdPtPi = new TF1("dndptPi","x*exp(-sqrt(x*x+1.949e-02)/[0])",ptmin,ptmax); // pion
  fdNdPtK  = new TF1("dndptK" ,"x*exp(-sqrt(x*x+0.493*0.493)/[0])",ptmin,ptmax); // kaon
  fdNdPtP  = new TF1("dndptP" ,"x*exp(-sqrt(x*x+0.938*0.938)/[0])",ptmin,ptmax); // proton
  fdNdPtPi->SetParameter(0,Tpi);
  fdNdPtK->SetParameter(0,TK);
  fdNdPtP->SetParameter(0,TP);
}

//_____________________________________________________________________
void KMCDetectorFwd::RequirePattern(UInt_t patt)
{
  // optional pattern to satyisfy
  if (!patt) return;
  int ngr = fPattITS.GetSize();
  fPattITS.Set(ngr+1);
  fPattITS[ngr] = patt;
}

//_____________________________________________________________________
void KMCDetectorFwd::BookControlHistos()
{
  fHChi2Branson = new TH1F("chi2Branson","chi2 Mu @ vtx",      100,0,100);
  fHChi2LrCorr = new TH2F("chi2Cl","chi2 corr cluster",        fNActiveLayersITS+1,0,fNActiveLayersITS+1,100,0,fMaxChi2Cl);
  fHChi2NDFCorr = new TH2F("chi2NDFCorr","chi2/ndf corr tr.",  fNActiveLayersITS+1,0,fNActiveLayersITS+1,100,0,fMaxNormChi2NDF);
  fHChi2NDFFake = new TH2F("chi2NDFFake","chi2/ndf fake tr.",  fNActiveLayersITS+1,0,fNActiveLayersITS+1,100,0,fMaxNormChi2NDF);
  fHChi2VtxCorr = new TH2F("chi2VCorr","chi2 to VTX corr tr." ,fNActiveLayersITS+1,0,fNActiveLayersITS+1,100,0,100);
  fHChi2VtxFake = new TH2F("chi2VFake","chi2 to VTX fake tr." ,fNActiveLayersITS+1,0,fNActiveLayersITS+1,100,0,100);
  fHNCand     = new TH2F("hNCand","Ncand per layer",           fNActiveLayersITS+1,0,fNActiveLayersITS+1,200,0,-1);
  fHCandCorID = new TH2F("CandCorID","Corr.cand ID per layer", fNActiveLayersITS+1,0,fNActiveLayersITS+1,200,0,-1);
  //
  fHChi2MS = new TH2F("chi2ms","chi2ms",100,0,30,10,0,10);
  //
}


//_____________________________________________________________________
Bool_t KMCDetectorFwd::IsCorrect(KMCProbeFwd *probTr)
{
  if (probTr->GetNFakeITSHits()) return kFALSE;
  return kTRUE;
}

//_____________________________________________________________________
void  KMCDetectorFwd::SetMinITSHits(int n)
{
  fMinITSHits = TMath::Min(n,fNActiveLayersITS);
}

//_____________________________________________________________________
void  KMCDetectorFwd::SetMinMSHits(int n)
{
  fMinMSHits = TMath::Min(n,fNActiveLayersMS);
}
//_____________________________________________________________________
void  KMCDetectorFwd::SetMinTRHits(int n)
{
  fMinTRHits = TMath::Min(n,fNActiveLayersTR);
}
