#include "string_fixed.h"
#include <stdlib.h>
#include <stdexcept>
#include <iomanip>
#include "glut.h"
#include "ScubaView.h"
#include "PreferencesManager.h"
#include "ScubaGlobalPreferences.h"
#include "Timer.h"
#include "Point2.h"
#include "Point3.h"
#include "VectorOps.h"
#include "Utilities.h"


using namespace std;

int const ScubaView::kBytesPerPixel = 4;
map<int,bool> ScubaView::mViewIDLinkedList;
Point3<float> ScubaView::mCursor( 0, 0, 0 );
int ScubaView::mcMarkers = 0;
int ScubaView::mNextMarker = -1;
std::map<int,Point3<float> > ScubaView::mMarkerRAS;
std::map<int,bool> ScubaView::mMarkerVisible;
ScubaViewStaticTclListener ScubaView::mStaticListener;

GLenum glError;
#define CheckGLError()  \
  glError = glGetError(); \
  while( glError != GL_NO_ERROR ) { \
    cerr << __LINE__ << " error: " << gluErrorString( glError ) << endl; \
    glError = glGetError(); \
  } \


int const ScubaView::kcInPlaneMarkerColors = 12;
float kInPlaneMarkerColors[ScubaView::kcInPlaneMarkerColors][3] = {
  { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 },
  { 1, 1, 0 }, { 1, 0, 1 }, { 0, 1, 1 }, 
  { 0.25, 0.75, 0 }, { 0.25, 0, 0.75 }, { 0, 0.25, 0.75 }, 
  { 0.75, 0.25, 0 }, { 0.75, 0, 0.25 }, { 0, 0.75, 0.25 } };

ScubaView::ScubaView() {
  mBuffer = NULL;
  mbPostRedisplay = false;
  mbRebuildOverlayDrawList = true;
  mViewIDLinkedList[GetID()] = false;
  mbFlipLeftRightInYZ = true;
  mbLockOnCursor = false;
  mInPlaneMovementIncrements[0] = 1.0;
  mInPlaneMovementIncrements[1] = 1.0;
  mInPlaneMovementIncrements[2] = 1.0;
  int nMarkerColor = GetID() % kcInPlaneMarkerColors;
  mInPlaneMarkerColor[0] = kInPlaneMarkerColors[nMarkerColor][0];
  mInPlaneMarkerColor[1] = kInPlaneMarkerColors[nMarkerColor][1];
  mInPlaneMarkerColor[2] = kInPlaneMarkerColors[nMarkerColor][2];
  mbVisibleInFrame = false;
  mCurrentMovingViewIntersection = -1;

  ScubaGlobalPreferences& globalPrefs =
    ScubaGlobalPreferences::GetPreferences();
  globalPrefs.AddListener( this );

  ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
  broadcaster.AddListener( this );

  mbFlipLeftRightInYZ = 
    globalPrefs.GetPrefAsBool( ScubaGlobalPreferences::ViewFlipLeftRight );

  // Try setting our initial transform to the default transform with
  // id 0. If it's not there, create it.
  try { 
    mWorldToView = &(ScubaTransform::FindByID( 0 ));
    mWorldToView->AddListener( this );
  }
  catch(...) {

    ScubaTransform* transform = new ScubaTransform();
    transform->SetLabel( "Identity" );
    
    try {
      mWorldToView = &(ScubaTransform::FindByID( 0 ));
      mWorldToView->AddListener( this );
    }
    catch(...) {
      DebugOutput( << "Couldn't make default transform!" );
    }
  }
  
  TclCommandManager& commandMgr = TclCommandManager::GetManager();
  commandMgr.AddCommand( *this, "SetViewInPlane", 2, "viewID inPlane",
			 "Sets the in plane in a view. inPlane should be "
			 "one of the following: x y z" );
  commandMgr.AddCommand( *this, "GetViewInPlane", 1, "viewID",
			 "Returns the in plane in a view." );
  commandMgr.AddCommand( *this, "SetViewZoomLevel", 2, "viewID zoomLevel",
			 "Sets the zoom level in a view. zoomLevel should be "
			 "a float." );
  commandMgr.AddCommand( *this, "SetViewRASCenter", 4, "viewID x y z",
			 "Sets the view center. x, y, and z should be floats "
			 "in world RAS coordinates." );
  commandMgr.AddCommand( *this, "SetLayerInViewAtLevel", 3, 
			 "viewID layerID level",
			 "Sets the layer in a view at a given draw level. "
			 "Higher draw levels will draw later." );
  commandMgr.AddCommand( *this, "GetLayerInViewAtLevel", 2, "viewID level",
			 "Returns the layer in a view at a given draw level.");
  commandMgr.AddCommand( *this, "RemoveAllLayersFromView", 1, "viewID",
			 "Remove all layers from a view." );
  commandMgr.AddCommand( *this, "RemoveLayerFromViewAtLevel", 2, 
			 "viewID layer",
			 "Remove a layer from a view." );
  commandMgr.AddCommand( *this, "GetLabelValuesSet", 2, "viewID setName",
			 "Get a set of label value pairs." );
  commandMgr.AddCommand( *this, "GetFirstUnusedDrawLevelInView", 1, "viewID",
			 "Returns the first unused draw level." );
  commandMgr.AddCommand( *this, "SetViewLinkedStatus", 2, "viewID linked",
			 "Set the linked status for a view." );
  commandMgr.AddCommand( *this, "GetViewLinkedStatus", 1, "viewID",
			 "Returns the linked status for a view." );
  commandMgr.AddCommand( *this, "SetViewLockOnCursor", 2, "viewID lock",
			 "Set a view to keep its view locked on the cursor." );
  commandMgr.AddCommand( *this, "GetViewLockOnCursor", 1, "viewID",
			 "Returns whether or not a view is locked on "
			 "the cursor." );
  commandMgr.AddCommand( *this, "SetViewTransform", 2, "viewID transformID",
			 "Set the view to world transform for a view." );
  commandMgr.AddCommand( *this, "GetViewTransform", 1, "viewID",
			 "Returns the transformID of a view's view to "
			 "world transform." );
  commandMgr.AddCommand( *this, "SetViewFlipLeftRightYZ", 2, "viewID flip",
			 "Set the left-right flip flag for a view." );
  commandMgr.AddCommand( *this, "GetViewFlipLeftRightYZ", 1, "viewID",
			 "Returns the left-right flip flag for a view." );
  commandMgr.AddCommand( *this, "SetViewInPlaneMovementIncrement", 3,
			 "viewID inPlane increment",
			 "Set the amount that using the in plane movement "
			 "keys will increment or decrement the in plane "
			 "RAS value. inPlane should be x, y, or z." );
  commandMgr.AddCommand( *this, "GetViewInPlaneMovementIncrement", 2,
			 "viewID inPlane",
			 "Returns the amount the in plane movement "
			 "increment for inPlane. inPlane should be x, "
			 "y, or z." );

  // Get some prefs values
  ScubaGlobalPreferences& prefs = ScubaGlobalPreferences::GetPreferences();
  msMoveViewLeft = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyMoveViewLeft );
  msMoveViewRight = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyMoveViewRight );
  msMoveViewUp = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyMoveViewUp );
  msMoveViewDown = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyMoveViewDown );
  msMoveViewIn = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyMoveViewIn );
  msMoveViewOut = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyMoveViewOut );
  msZoomViewIn = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyZoomViewIn );
  msZoomViewOut = 
    prefs.GetPrefAsString( ScubaGlobalPreferences::KeyZoomViewOut );
  

  map<string,string> labelValueMap;
  mLabelValueMaps["mouse"] = labelValueMap;
  mLabelValueMaps["cursor"] = labelValueMap;
}

ScubaView::~ScubaView() {
}

void
ScubaView::Set2DRASCenter ( float iRASCenter[3] ) {

  mViewState.mCenterRAS[0] = iRASCenter[0];
  mViewState.mCenterRAS[1] = iRASCenter[1];
  mViewState.mCenterRAS[2] = iRASCenter[2];
    
  // Broadcast this change.
  ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
  broadcaster.SendBroadcast( "2DRASCenterChanged", (void*)&mID );
  
  CalcViewToWindowTransform();

  // Changed our view center, so we need to rebuild the overlay.
  RebuildOverlayDrawList();
  RequestRedisplay();
}

void
ScubaView::Set2DZoomLevel ( float iZoomLevel ) {

  mViewState.mZoomLevel = iZoomLevel;

  // Broadcast this change.
  ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
  broadcaster.SendBroadcast( "2DZoomLevelChanged", (void*)&mID );

  // Changed zoom, so we need to rebuild the overlay.
  RebuildOverlayDrawList();
  RequestRedisplay();
}

void
ScubaView::Set2DInPlane ( ViewState::Plane iPlane ) {

  // If we are going to a new plane, reset our plane normal. 
  if( mViewState.mInPlane != iPlane ) {
    switch( iPlane ) {
    case ViewState::X:
      mViewState.mPlaneNormal[0] = 1; 
      mViewState.mPlaneNormal[1] = 0;
      mViewState.mPlaneNormal[2] = 0;
      break;
    case ViewState::Y:
      mViewState.mPlaneNormal[0] = 0;
      mViewState.mPlaneNormal[1] = 1;
      mViewState.mPlaneNormal[2] = 0;
      break;
    case ViewState::Z:
      mViewState.mPlaneNormal[0] = 0; 
      mViewState.mPlaneNormal[1] = 0;
      mViewState.mPlaneNormal[2] = 1;
      break;
    }
  }

  mViewState.mInPlane = iPlane;

  // Broadcast this change.
  ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
  broadcaster.SendBroadcast( "2DInPlaneChanged", (void*)&mID );

  CalcViewToWindowTransform();

  // We changed our orientation so we must recalc all the other views'
  // intersection points.
  CalcAllViewIntersectionPoints();

  // Changed in plane, so we need to rebuild the overlay.
  RebuildOverlayDrawList();
  RequestRedisplay();
}

void
ScubaView::Set2DPlaneNormalOrthogonalToInPlane () {

  // Reset our plane normal. 
  float plane[3];
  switch( mViewState.mInPlane ) {
  case ViewState::X:
    plane[0] = 1; 
    plane[1] = 0;
    plane[2] = 0;
    break;
  case ViewState::Y:
    plane[0] = 0;
    plane[1] = 1;
    plane[2] = 0;
    break;
  case ViewState::Z:
    plane[0] = 0; 
    plane[1] = 0;
    plane[2] = 1;
    break;
  }

  Set2DPlaneNormal( plane );
}

void
ScubaView::Set2DPlaneNormal ( float iNormal[3] ) {

  switch( mViewState.mInPlane ) {
  case ViewState::X:
    if( fabs(iNormal[0]) >= 0.5 ) {
      mViewState.mPlaneNormal[0] = iNormal[0];
      mViewState.mPlaneNormal[1] = iNormal[1];
      mViewState.mPlaneNormal[2] = iNormal[2];
    }
    break;
  case ViewState::Y:
    if( fabs(iNormal[1]) >= 0.5 ) {
      mViewState.mPlaneNormal[0] = iNormal[0];
      mViewState.mPlaneNormal[1] = iNormal[1];
      mViewState.mPlaneNormal[2] = iNormal[2];
    }
    break;
  case ViewState::Z:
    if( fabs(iNormal[2]) >= 0.5 ) {
      mViewState.mPlaneNormal[0] = iNormal[0];
      mViewState.mPlaneNormal[1] = iNormal[1];
      mViewState.mPlaneNormal[2] = iNormal[2];
    }
    break;
  }

  // Broadcast this change.
  ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
  broadcaster.SendBroadcast( "2DPlaneNormalChanged", (void*)&mID );

  CalcViewToWindowTransform();

  // We changed our orientation so we must recalc all the other views'
  // intersection points.
  CalcAllViewIntersectionPoints();

  // Changed in plane, so we need to rebuild the overlay.
  RebuildOverlayDrawList();
  RequestRedisplay();
}

void
ScubaView::Get2DRASCenter ( float oRASCenter[3] ) {

  oRASCenter[0] = mViewState.mCenterRAS[0];
  oRASCenter[1] = mViewState.mCenterRAS[1];
  oRASCenter[2] = mViewState.mCenterRAS[2];
}

float
ScubaView::Get2DZoomLevel () {
  
  return mViewState.mZoomLevel;
}

ViewState::Plane
ScubaView::Get2DInPlane () {

  return mViewState.mInPlane;
}

void
ScubaView::Get2DPlaneNormal ( float oNormal[3] ) {

  oNormal[0] = mViewState.mPlaneNormal[0];
  oNormal[1] = mViewState.mPlaneNormal[1];
  oNormal[2] = mViewState.mPlaneNormal[2];
}


void 
ScubaView::SetLayerAtLevel ( int iLayerID, int iLevel ) {

  // If we got layer ID -1, remove this layer. Otherwise set it in our
  // map.
  if( iLayerID == -1 ) {

    RemoveLayerAtLevel( iLevel );
    
  } else {
    
    try {
      Layer& layer = Layer::FindByID( iLayerID );
      
      mLevelLayerIDMap[iLevel] = iLayerID;

      // Set the layer's width and height.
      layer.SetWidth( mWidth );
      layer.SetHeight( mHeight );

      // If this is level 0, get our in plane increments from it.
      if( 0 == iLevel ) {
	layer.GetPreferredInPlaneIncrements( mInPlaneMovementIncrements );
      }

      // Listen to it.
      layer.AddListener( this );

    }
    catch(...) {
      DebugOutput( << "Couldn't find layer " << iLayerID );
    }
  }

  RequestRedisplay();
}

int
ScubaView::GetLayerAtLevel ( int iLevel ) {

  // Look for this level in the level layer ID map. If we found
  // it, return the layer ID, otherwise return -1.
  map<int,int>::iterator tLevelLayerID = mLevelLayerIDMap.find( iLevel );
  if( tLevelLayerID == mLevelLayerIDMap.end() ) {
    return -1;
  } else {
    return mLevelLayerIDMap[iLevel];
  }
}

void 
ScubaView::RemoveAllLayers () {

  mLevelLayerIDMap.clear();
}

void 
ScubaView::RemoveLayerAtLevel ( int iLevel ) {

  mLevelLayerIDMap.erase( iLevel );
}

void 
ScubaView::CopyLayerSettingsToView ( ScubaView& iView ) {

  map<int,int>::iterator tLevelLayerID;
  for( tLevelLayerID = mLevelLayerIDMap.begin(); 
       tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {
    
    int level = (*tLevelLayerID).first;
    int layerID = (*tLevelLayerID).second;
    iView.SetLayerAtLevel( level, layerID );
  }
}

void
ScubaView::SetWorldToViewTransform ( int iTransformID ) {

  try {
    mWorldToView->RemoveListener( this );
    mWorldToView = &(ScubaTransform::FindByID( iTransformID ));
    mWorldToView->AddListener( this );
    CalcWorldToWindowTransform();
    RequestRedisplay();
  }
  catch(...) {
    DebugOutput( << "Couldn't find transform " << iTransformID );
  }
}

int
ScubaView::GetWorldToViewTransform () {

  return mWorldToView->GetID();
}


TclCommandListener::TclCommandResult
ScubaView::DoListenToTclCommand( char* isCommand, int iArgc, char** iasArgv ) {

  // SetViewInPlane <viewID> <inPlane>
  if( 0 == strcmp( isCommand, "SetViewInPlane" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      ViewState::Plane inPlane;
      if( 0 == strcmp( iasArgv[2], "X" ) || 
	  0 == strcmp( iasArgv[2], "x" ) ||
	  0 == strcmp( iasArgv[2], "R" ) || 
	  0 == strcmp( iasArgv[2], "r" ) ) {
	inPlane = ViewState::X;
      } else if( 0 == strcmp( iasArgv[2], "Y" ) || 
		 0 == strcmp( iasArgv[2], "y" ) ||
		 0 == strcmp( iasArgv[2], "A" ) || 
		 0 == strcmp( iasArgv[2], "a" ) ) {
	inPlane = ViewState::Y;
      } else if( 0 == strcmp( iasArgv[2], "Z" ) || 
		 0 == strcmp( iasArgv[2], "z" ) ||
		 0 == strcmp( iasArgv[2], "S" ) || 
		 0 == strcmp( iasArgv[2], "s" )) {
	inPlane = ViewState::Z;
      } else {
	sResult = "bad inPlane \"" + string(iasArgv[2]) + 
	  "\", should be x, y, or z, or r, a, or s";
	return error;
      }
      Set2DInPlane( inPlane );
    }
  }

  // GetViewInPlane <viewID>
  if( 0 == strcmp( isCommand, "GetViewInPlane" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      sReturnFormat = "s";
      switch( mViewState.mInPlane ) {
      case ViewState::X:
	sReturnValues = "x";
	break;
      case ViewState::Y:
	sReturnValues = "y";
	break;
      case ViewState::Z:
	sReturnValues = "z";
	break;
      }
    }
  }

  // SetViewZoomLevel <viewID> <zoomLevel>
  if( 0 == strcmp( isCommand, "SetViewZoomLevel" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      float zoomLevel = (float) strtod( iasArgv[2], (char**)NULL );
      if( ERANGE == errno ) {
	sResult = "bad zoom level";
	return error;
      }
      
      Set2DZoomLevel( zoomLevel );
    }
  }

  // SetViewRASCenter <viewID> <X> <Y> <Z>
  if( 0 == strcmp( isCommand, "SetViewRASCenter" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }

    if( mID == viewID ) {
      
      float x = (float) strtod( iasArgv[2], (char**)NULL );
      if( ERANGE == errno ) {
	sResult = "bad x coordinate";
	return error;
      }
      float y = (float) strtod( iasArgv[3], (char**)NULL );
      if( ERANGE == errno ) {
	sResult = "bad y coordinate";
	return error;
      }
      float z = (float) strtod( iasArgv[4], (char**)NULL );
      if( ERANGE == errno ) {
	sResult = "bad z coordinate";
	return error;
      }
      
      float center[3];
      center[0] = x; center[1] = y; center[2] = z;
      Set2DRASCenter( center );
    }
  }

  // SetLayerInViewAtLevel <viewID> <layerID> <level>
  if( 0 == strcmp( isCommand, "SetLayerInViewAtLevel" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      int layerID = strtol( iasArgv[2], (char**)NULL, 10 );
      if( ERANGE == errno ) {
	sResult = "bad layer ID";
	return error;
      }
      int level = strtol( iasArgv[3], (char**)NULL, 10 );
      if( ERANGE == errno ) {
	sResult = "bad level";
	return error;
      }
      
      SetLayerAtLevel( layerID, level );
    }
  }

  // GetLayerInViewAtLevel <viewID> <level>
  if( 0 == strcmp( isCommand, "GetLayerInViewAtLevel" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      int level = strtol( iasArgv[2], (char**)NULL, 10 );
      if( ERANGE == errno ) {
	sResult = "bad level";
	return error;
      }
      
      stringstream ssReturn;
      sReturnFormat = "i";
      ssReturn << GetLayerAtLevel( level );
      sReturnValues = ssReturn.str();
      return ok;
    }
  }

  // RemoveLayerFromViewAtLevel <viewID> <level>
  if( 0 == strcmp( isCommand, "RemoveLayerFromViewAtLevel" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      int level = strtol( iasArgv[2], (char**)NULL, 10 );
      if( ERANGE == errno ) {
	sResult = "bad level";
	return error;
      }
      
      RemoveLayerAtLevel( level );
    }
  }

  // RemoveAllLayersFromView <viewID>
  if( 0 == strcmp( isCommand, "RemoveAllLayersFromView" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      RemoveAllLayers();
    }
  }

  // GetLabelValuesSet <viewID> <setName>
  if( 0 == strcmp( isCommand, "GetLabelValuesSet" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {

      string sSetName = iasArgv[2];

      map<string,map<string,string> >::iterator tMap = 
	mLabelValueMaps.find( sSetName );
      if( tMap == mLabelValueMaps.end() ) {
	stringstream ssResult;
	ssResult << "Set name " << sSetName << " doesn't exist.";
	sResult = ssResult.str();
	return error;
      }

      map<string,string> labelValueMap = mLabelValueMaps[sSetName];
      map<string,string>::iterator tLabelValue;

      stringstream ssFormat;
      stringstream ssResult;
      ssFormat << "L";

      for( tLabelValue = labelValueMap.begin(); 
	   tLabelValue != labelValueMap.end(); ++tLabelValue ) {
	
	ssFormat << "Lssl";
	ssResult << "\"" << (*tLabelValue).first << "\" \"" 
		 << (*tLabelValue).second << "\" ";
      }
      ssFormat << "l";
      
      sReturnFormat = ssFormat.str();
      sReturnValues = ssResult.str();
    }
  }

  // GetFirstUnusedDrawLevelInView <viewID>
  if( 0 == strcmp( isCommand, "GetFirstUnusedDrawLevelInView" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {

      sReturnFormat = "i";
      stringstream ssReturnValues;
      ssReturnValues << GetFirstUnusedDrawLevel();
      sReturnValues = ssReturnValues.str();
    }
  }


  // SetViewLinkedStatus <viewID> <linked>
  if( 0 == strcmp( isCommand, "SetViewLinkedStatus" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      if( 0 == strcmp( iasArgv[2], "true" ) || 
	  0 == strcmp( iasArgv[2], "1" )) {
	SetLinkedStatus( true );
      } else if( 0 == strcmp( iasArgv[2], "false" ) ||
		 0 == strcmp( iasArgv[2], "0" ) ) {
	SetLinkedStatus( false );
      } else {
	sResult = "bad linkedStatus \"" + string(iasArgv[2]) +
	  "\", should be true, 1, false, or 0";
	return error;	
      }
    }
  }

  // GetViewLinkedStatus <viewID>
  if( 0 == strcmp( isCommand, "GetViewLinkedStatus" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {

      sReturnFormat = "i";
      stringstream ssReturnValues;
      ssReturnValues << (int)GetLinkedStatus();
      sReturnValues = ssReturnValues.str();
    }
  }

  // SetViewLockOnCursor <viewID> <lock>
  if( 0 == strcmp( isCommand, "SetViewLockOnCursor" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      if( 0 == strcmp( iasArgv[2], "true" ) || 
	  0 == strcmp( iasArgv[2], "1" )) {
	SetLockOnCursor( true );
      } else if( 0 == strcmp( iasArgv[2], "false" ) ||
		 0 == strcmp( iasArgv[2], "0" ) ) {
	SetLockOnCursor( false );
      } else {
	sResult = "bad lock \"" + string(iasArgv[2]) +
	  "\", should be true, 1, false, or 0";
	return error;	
      }
    }
  }

  // GetViewLockOnCursor <viewID>
  if( 0 == strcmp( isCommand, "GetViewLockOnCursor" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {

      sReturnFormat = "i";
      stringstream ssReturnValues;
      ssReturnValues << (int)GetLockOnCursor();
      sReturnValues = ssReturnValues.str();
    }
  }

  // SetViewTransform <viewID> <transformID>
  if( 0 == strcmp( isCommand, "SetViewTransform" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      int transformID = strtol( iasArgv[2], (char**)NULL, 10 );
      if( ERANGE == errno ) {
	sResult = "bad transformID";
	return error;
      }
      
      SetWorldToViewTransform( transformID );
    }
  }

  // GetViewTransform <viewID>
  if( 0 == strcmp( isCommand, "GetViewTransform" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {

      sReturnFormat = "i";
      stringstream ssReturnValues;
      ssReturnValues << (int)GetWorldToViewTransform();
      sReturnValues = ssReturnValues.str();
    }
  }

  // SetViewFlipLeftRightYZ <viewID> <flip>
  if( 0 == strcmp( isCommand, "SetViewFlipLeftRightYZ" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      if( 0 == strcmp( iasArgv[2], "true" ) || 
	  0 == strcmp( iasArgv[2], "1" )) {
	SetFlipLeftRightYZ( true );
      } else if( 0 == strcmp( iasArgv[2], "false" ) ||
		 0 == strcmp( iasArgv[2], "0" ) ) {
	SetFlipLeftRightYZ( false );
      } else {
	sResult = "bad flip value \"" + string(iasArgv[2]) +
	  "\", should be true, 1, false, or 0";
	return error;	
      }
    }
  }

  // GetViewFlipLeftRightYZ <viewID>
  if( 0 == strcmp( isCommand, "GetViewFlipLeftRightYZ" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {

      sReturnFormat = "i";
      stringstream ssReturnValues;
      ssReturnValues << (int)GetFlipLeftRightYZ();
      sReturnValues = ssReturnValues.str();
    }
  }

  // SetViewInPlaneMovementIncrement <viewID> <inPlane> <increment>
  if( 0 == strcmp( isCommand, "SetViewInPlaneMovementIncrement" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {
      
      float increment = (float) strtod( iasArgv[3], (char**)NULL );
      if( ERANGE == errno ) {
	sResult = "bad increment";
	return error;
      }
      
      if( 0 == strcmp( iasArgv[2], "x" ) ||
	  0 == strcmp( iasArgv[2], "X" ) ) {

	mInPlaneMovementIncrements[0] = increment;

      } else if( 0 == strcmp( iasArgv[2], "y" ) ||
		 0 == strcmp( iasArgv[2], "Y" ) ) {

	mInPlaneMovementIncrements[1] = increment;

      } else if( 0 == strcmp( iasArgv[2], "z" ) ||
		 0 == strcmp( iasArgv[2], "Z" ) ) {

	mInPlaneMovementIncrements[2] = increment;

      } else {
	stringstream ssResult;
	ssResult << "bad inPlane \"" << iasArgv[1] << "\", should be "
		 << "x, y, or z.";
	sResult = ssResult.str();
	return error;
      }
    }
  }

  // GetViewInPlaneMovementIncrement <viewID> <inPlane>
  if( 0 == strcmp( isCommand, "GetViewInPlaneMovementIncrement" ) ) {
    int viewID = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad view ID";
      return error;
    }
    
    if( mID == viewID ) {

      float increment = 0;
      if( 0 == strcmp( iasArgv[2], "x" ) ||
	  0 == strcmp( iasArgv[2], "X" ) ) {

	increment = mInPlaneMovementIncrements[0];

      } else if( 0 == strcmp( iasArgv[2], "y" ) ||
		 0 == strcmp( iasArgv[2], "Y" ) ) {

	increment = mInPlaneMovementIncrements[1];

      } else if( 0 == strcmp( iasArgv[2], "z" ) ||
		 0 == strcmp( iasArgv[2], "Z" ) ) {

	increment = mInPlaneMovementIncrements[2];

      } else {
	stringstream ssResult;
	ssResult << "bad inPlane \"" << iasArgv[2] << "\", should be "
		 << "x, y, or z.";
	sResult = ssResult.str();
	return error;
      }

      sReturnFormat = "f";
      stringstream ssReturnValues;
      ssReturnValues << increment;
      sReturnValues = ssReturnValues.str();
    }
  }

  return ok;
}

void
ScubaView::DoListenToMessage ( string isMessage, void* iData ) {
  
  if( isMessage == "transformChanged" ) {
    RebuildOverlayDrawList(); // our overlay coords are different now
    RequestRedisplay();
  }

  if( isMessage == "layerChanged" ) {
    RequestRedisplay();
    SendBroadcast( "viewChanged", NULL );
  }

  if( isMessage == "cursorChanged" ) {

    // If we're locked on the cursor, set our view now.
    if( mbLockOnCursor ) {
      Set2DRASCenter( mCursor.xyz() );
    }

    mbRebuildOverlayDrawList = true;
    RequestRedisplay();
  }

  if( isMessage == "markerChanged" ) {
    mbRebuildOverlayDrawList = true;
    RequestRedisplay();
  }

  if( isMessage == "DrawCoordinateOverlay" ||
      isMessage == "DrawPlaneIntersections" ) {
    RebuildOverlayDrawList(); // our overlay will be different
    RequestRedisplay();
  }

  if( isMessage == "2DRASCenterChanged" ||
      isMessage == "2DZoomLevelChanged" ||
      isMessage == "2DInPlaneChanged" ||
      isMessage == "2DPlaneNormalChanged" ) {

    int viewID = *(int*)iData;

    // If somebody else's RAS center or inplane changed, we need to
    // recalc our inplane intersection points.
    CalcViewIntersectionPoints( viewID );

    // If we're linked, we need to change our view.
    if( mViewIDLinkedList[GetID()] && mViewIDLinkedList[viewID] ) {
      View& view = View::FindByID( viewID );
      // ScubaView& scubaView = dynamic_cast<ScubaView&>(view);
      ScubaView& scubaView = (ScubaView&)view;

      // Change center or zoom level if linked. Don't link the in plane.
      if( isMessage == "2DRASCenterChanged" ) {
	float RASCenter[3];
	scubaView.Get2DRASCenter( RASCenter );
	Set2DRASCenter( RASCenter );
      } else if ( isMessage == "2DZoomLevelChanged" ) {
	float zoomLevel;
	zoomLevel = scubaView.Get2DZoomLevel();
	Set2DZoomLevel( zoomLevel );
      }
    }

    // If we're visible request redisplays.
    if( IsVisibleInFrame() ) {
      mbRebuildOverlayDrawList = true;
      RequestRedisplay();
    }
  }

  // We cache these values but down have to act on them right away.
  if( isMessage == "KeyMoveViewLeft" )  msMoveViewLeft  = *(string*)iData;
  if( isMessage == "KeyMoveViewRight" ) msMoveViewRight = *(string*)iData;
  if( isMessage == "KeyMoveViewUp" )    msMoveViewUp    = *(string*)iData;
  if( isMessage == "KeyMoveViewDown" )  msMoveViewDown  = *(string*)iData;
  if( isMessage == "KeyMoveViewIn" )    msMoveViewIn    = *(string*)iData;
  if( isMessage == "KeyMoveViewOut" )   msMoveViewOut   = *(string*)iData;
  if( isMessage == "KeyZoomViewIn" )    msZoomViewIn    = *(string*)iData;
  if( isMessage == "KeyZoomViewOut" )   msZoomViewOut   = *(string*)iData;

  // New view, get some info about it.
  if( isMessage == "NewView" ) {
    int viewID = *(int*)iData;
    if( viewID != GetID() ) {
      CalcViewIntersectionPoints( viewID );
    }
  }

  View::DoListenToMessage( isMessage, iData );
}

void
ScubaView::DoDraw() {

#if 1
  ::Timer timer;
  timer.Start();
#endif

  BuildFrameBuffer();
  DrawFrameBuffer();
  DrawOverlay();

#if 1
  int msec = timer.TimeNow();
  float fps = 1.0 / ((float)msec/1000.0);

  stringstream ssCommand;
  ssCommand << "SetStatusBarText \"" << fps << " fps\"";

  TclCommandManager& mgr = TclCommandManager::GetManager();
  mgr.SendCommand( ssCommand.str() );
#endif
}

void
ScubaView::DoReshape( int iWidth, int iHeight ) {

  if( iWidth < 0 || iHeight < 0 ) {
    stringstream sError;
    sError << "Invalid width " << mWidth << " or height " << mHeight;
    DebugOutput( << sError.str() );
    throw new runtime_error( sError.str() );
  }

  // Set the view state buffer height and width.
  mViewState.mBufferWidth  = iWidth;
  mViewState.mBufferHeight = iHeight;

  // Allocate a new buffer.
  GLubyte* newBuffer = (GLubyte*) malloc( mWidth * mHeight * kBytesPerPixel );
  if( NULL == newBuffer ) {
    stringstream sError;
    sError << "Couldn't allocate buffer for width " << mWidth 
	   << " height " << mHeight;
    DebugOutput( << sError.str() );
    throw new runtime_error( sError.str() );
  }

  // Get rid of the old one.
  if( NULL != mBuffer ) {
    free( mBuffer );
  }
  
  // Save the new one.
  mBuffer = newBuffer;

  // Set the width and height in all the layers.
  map<int,int>::iterator tLevelLayerID;
  for( tLevelLayerID = mLevelLayerIDMap.begin(); 
       tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {
    
    int layerID = (*tLevelLayerID).second;

    try {
      Layer& layer = Layer::FindByID( layerID );
      
      layer.SetWidth( mWidth );
      layer.SetHeight( mHeight );
    }
    catch(...) {
      DebugOutput( << "Couldn't find layer " << layerID );
    }
  }

  mbRebuildOverlayDrawList = true;
}

void
ScubaView::DoTimer() {

}
  
void
ScubaView::DoMouseMoved( int iWindow[2], 
			 InputState& iInput, ScubaToolState& iTool ) {

  float ras[3];
  TranslateWindowToRAS( iWindow, ras );

  // Rebuild our label value info because the mouse has moved.
  RebuildLabelValueInfo( ras, "mouse" );


  // Handle the navigation tool and plane tool.
  if( iTool.GetMode() == ScubaToolState::navigation ||
      iTool.GetMode() == ScubaToolState::plane ) {

    if( iInput.Button() && 
	!iInput.IsControlKeyDown() && !iInput.IsShiftKeyDown() ) {
      
      float delta[2];
      delta[0] = (float)(mLastMouseMoved[0] - iWindow[0]) / 
	mViewState.mZoomLevel;
      delta[1] = (float)(mLastMouseMoved[1] - iWindow[1]) /
	mViewState.mZoomLevel;
      
      /* add to the total delta */
      mMouseMoveDelta[0] += delta[0];
      mMouseMoveDelta[1] += delta[1];
      
      /* save this mouse position */
      mLastMouseMoved[0] = iWindow[0];
      mLastMouseMoved[1] = iWindow[1];
      
      float moveLeftRight = 0, moveUpDown = 0, moveInOut = 0, zoomInOut = 0;
      switch( iInput.Button() ) {
      case 1:
	moveLeftRight = mMouseMoveDelta[0];
	moveUpDown    = mMouseMoveDelta[1];
	break;
      case 2:
	moveInOut = mMouseMoveDelta[1];
	break;
      case 3:
	zoomInOut = mMouseMoveDelta[1] / 10.0;
	break;
      }

      if( mbFlipLeftRightInYZ && 
	  (mViewState.mInPlane == ViewState::Y ||
	   mViewState.mInPlane == ViewState::Z) ) {
	moveLeftRight = -moveLeftRight;
      }
      
      if( moveLeftRight || moveUpDown || moveInOut ) {
	
	if( iTool.GetMode() == ScubaToolState::navigation ) {

	  // Put the window relative translations into window relative.
	  Point3<float> move;
	  switch( mViewState.mInPlane ) {
	  case ViewState::X:
	    move.Set( moveInOut, moveLeftRight, moveUpDown );
	    break;
	  case ViewState::Y:
	    move.Set( moveLeftRight, moveInOut, moveUpDown );
	    break;
	  case ViewState::Z:
	    move.Set( moveLeftRight, moveUpDown, moveInOut );
	    break;
	  }
	  
	  // Do the move.
	  Point3<float> newCenterRAS;
	  TranslateRASInWindowSpace( mOriginalCenterRAS, move.xyz(), 
				     newCenterRAS.xyz() );

	  // Set the new center.
	  Set2DRASCenter( newCenterRAS.xyz() );

	} else if( iTool.GetMode() == ScubaToolState::plane ) {

	  // If we have a view to move...
	  if( mCurrentMovingViewIntersection != -1 ) {
	    
	    // Get the view..
	    View& view = View::FindByID( mCurrentMovingViewIntersection );
	    // ScubaView& scubaView = dynamic_cast<ScubaView&>(view);
	    ScubaView& scubaView = (ScubaView&)view;
	    
	    switch( iInput.Button() ) {
	      
	      // Button 1, move that view's center RAS.
	    case 1: {
	      
	      // Put the window relative translations into window
	      // relative. Note we don't move in or out in this tool.
	      Point3<float> move;
	      switch( mViewState.mInPlane ) {
	      case ViewState::X:
		move.Set( 0, -moveLeftRight, -moveUpDown );
		break;
	      case ViewState::Y:
		move.Set( -moveLeftRight, 0, -moveUpDown );
		break;
	      case ViewState::Z:
		move.Set( -moveLeftRight, -moveUpDown, 0 );
		break;
	      }
	      
	      // Do the move.
	      Point3<float> newCenterRAS;
	      TranslateRASInWindowSpace( mOriginalCenterRAS, move.xyz(), 
					 newCenterRAS.xyz() );

	      // Set the new center in the view.
	      scubaView.Set2DRASCenter( newCenterRAS.xyz() );
	      
	    } break;
	    
	    // Button 2, rotate.
	    case 2: {
	
	      // Find the biggest delta and divide it by the screen
	      // dimension.
	      float delta = (fabs(mMouseMoveDelta[0]) >
			     fabs(mMouseMoveDelta[1]) ?
			     mMouseMoveDelta[0] / (float)mWidth :
			     mMouseMoveDelta[1] / (float)mHeight);
	      
	      // Multiple by ~2pi to get radians.
	      float rads = delta * 6.3;
	      
	      // We're going to rotate around our plane normal. With
	      // an origin of 0.
	      Point3<float> axis( mViewState.mPlaneNormal );
	      Point3<float> origin( 0,0,0 );
	      Matrix44 rotate;
	      rotate.MakeRotation( origin.xyz(), axis.xyz(), rads );
	      
	      // Now rotate the original normal to get a new normal.
	      Point3<float> newPlaneNormal;
	      rotate.MultiplyVector3( mOriginalPlaneNormal.xyz(),
				      newPlaneNormal.xyz() );
	      
	      // Normalize it and set it in the view.
	      NormalizeVector( newPlaneNormal );
	      scubaView.Set2DPlaneNormal( newPlaneNormal.xyz() );
	    } break;
	    }
	  }
	}
      }
      
      if( zoomInOut ) {
	
	float newZoom = mOriginalZoom + zoomInOut;
	if( newZoom <= 0.25 ) {
	  newZoom = 0.25;
	}
	Set2DZoomLevel( newZoom );
      }
    }
  }
  
  // If not a straight control-click, pass this tool to our layers.
  if( !(iInput.IsControlKeyDown() && 
	!iInput.IsShiftKeyDown() && !iInput.IsAltKeyDown()) ) {
    map<int,int>::iterator tLevelLayerID;
    for( tLevelLayerID = mLevelLayerIDMap.begin(); 
	 tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {
      int layerID = (*tLevelLayerID).second;
      try {
	Layer& layer = Layer::FindByID( layerID );
	layer.HandleTool( ras, mViewState, *this, iTool, iInput );
	if( layer.WantRedisplay() ) {
	  RequestRedisplay();
	  layer.RedisplayPosted();
	}
      }
      catch(...) {
	DebugOutput( << "Couldn't find layer " << layerID );
      }
    }
  }
}

void
  ScubaView::DoMouseUp( int iWindow[2],
		      InputState& iInput, ScubaToolState& iTool ) {

  // No matter what tool we're on, look for ctrl-b{1,2,3} and do some
  // navigation stuff.
  if( iInput.IsControlKeyDown() && 
      !iInput.IsShiftKeyDown() && !iInput.IsAltKeyDown() ) {
    
    // Set the new view center to this point. If they also hit b1 or
    // b3, zoom in or out accordingly.
    float world[3];
    TranslateWindowToRAS( iWindow, world );
    Set2DRASCenter( world );

    switch( iInput.Button() ) {
    case 1:
      mViewState.mZoomLevel *= 2.0;
      break;
    case 3:
      mViewState.mZoomLevel /= 2.0;
      if( mViewState.mZoomLevel < 0.25 ) {
	mViewState.mZoomLevel = 0.25;
      }
      break;
    }

    mbRebuildOverlayDrawList = true;
    RequestRedisplay();
  }

  // Handle marker tool.
  if( iTool.GetMode() == ScubaToolState::marker && 
      !iInput.IsControlKeyDown() ) {

    float world[3];
    TranslateWindowToRAS( iWindow, world );

    switch( iInput.Button() ) {
    case 1:
      SetCursor( world );
      RebuildLabelValueInfo( mCursor.xyz(), "cursor" );
      break;
    case 2:
      SetNextMarker( world );
      break;
    case 3:
      HideNearestMarker( world );
      break;
    }
  }

  // Unselect view intersection line if plane tool.
  if( iTool.GetMode() == ScubaToolState::plane ) {
    
    // If we clicked with button 3, set the plane to its orthogonal
    // position.
    if( mCurrentMovingViewIntersection != -1 && 
	iInput.Button() == 3 ) {

      // Get the view..
      View& view = View::FindByID( mCurrentMovingViewIntersection );
      // ScubaView& scubaView = dynamic_cast<ScubaView&>(view);
      ScubaView& scubaView = (ScubaView&)view;

      scubaView.Set2DPlaneNormalOrthogonalToInPlane();
    }

    mCurrentMovingViewIntersection = -1;
    mbRebuildOverlayDrawList = true;
    RequestRedisplay();
  }

  // If not a straight control-click, pass this tool to our layers.
  if( !(iInput.IsControlKeyDown() && 
	!iInput.IsShiftKeyDown() && !iInput.IsAltKeyDown()) ) {
    float ras[3];
    TranslateWindowToRAS( iWindow, ras );
    map<int,int>::iterator tLevelLayerID;
    for( tLevelLayerID = mLevelLayerIDMap.begin(); 
	 tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {
      int layerID = (*tLevelLayerID).second;
      try {
	Layer& layer = Layer::FindByID( layerID );
	layer.HandleTool( ras, mViewState, *this, iTool, iInput );
	if( layer.WantRedisplay() ) {
	  RequestRedisplay();
	  layer.RedisplayPosted();
	}
      }
      catch(...) {
	DebugOutput( << "Couldn't find layer " << layerID );
      }
    }
  }
}

void
ScubaView::DoMouseDown( int iWindow[2], 
			InputState& iInput, ScubaToolState& iTool ) {

  Point3<float> ras;
  TranslateWindowToRAS( iWindow, ras.xyz() );

  mLastMouseDown[0] = mLastMouseMoved[0] = iWindow[0];
  mLastMouseDown[1] = mLastMouseMoved[1] = iWindow[1];
  mMouseMoveDelta[0] = 0.0;
  mMouseMoveDelta[1] = 0.0;
  mOriginalCenterRAS[0] = mViewState.mCenterRAS[0];
  mOriginalCenterRAS[1] = mViewState.mCenterRAS[1];
  mOriginalCenterRAS[2] = mViewState.mCenterRAS[2];
  mOriginalZoom = mViewState.mZoomLevel;

  // If this is the plane tool, find the nearest plane line.
  if( iTool.GetMode() == ScubaToolState::plane ) {

    // Find the closest inplane line from another view.
    float minDistance = 999999;
    int closestViewID = -1;
    list<int> viewIDs;
    GetIDList( viewIDs );
    list<int>::iterator tViewID;
    for( tViewID = viewIDs.begin(); tViewID != viewIDs.end(); ++tViewID ) {
      
      int viewID = *tViewID;
      if( viewID != GetID () ) {
	
	View& view = View::FindByID( viewID );
	// ScubaView& scubaView = dynamic_cast<ScubaView&>(view);
	ScubaView& scubaView = (ScubaView&)view;
	
	if( scubaView.IsVisibleInFrame() ) {
	  
	  Point3<float> P1, P2;
	  P1.Set( mViewIDViewIntersectionPointMap[viewID][0] );
	  P2.Set( mViewIDViewIntersectionPointMap[viewID][1] );
	  
	  float distance = 
	    Utilities::DistanceFromLineToPoint3f( P1, P2, ras );
	  if( distance <= minDistance ) {
	    minDistance = distance;
	    closestViewID = viewID;
	  }
	}
      }
    }

    mCurrentMovingViewIntersection = closestViewID;
    if( mCurrentMovingViewIntersection != -1 ) {
      // Get the views current RAS center.
      View& view = View::FindByID( mCurrentMovingViewIntersection );
      // ScubaView& scubaView = dynamic_cast<ScubaView&>(view);
      ScubaView& scubaView = (ScubaView&)view;
      scubaView.Get2DRASCenter( mOriginalCenterRAS );
      scubaView.Get2DPlaneNormal( mOriginalPlaneNormal.xyz() );

      mbRebuildOverlayDrawList = true;
      RequestRedisplay();
    }
  }

  // If not a straight control-click, pass this tool to our layers.
  if( !(iInput.IsControlKeyDown() && 
	!iInput.IsShiftKeyDown() && !iInput.IsAltKeyDown()) ) {
    map<int,int>::iterator tLevelLayerID;
    for( tLevelLayerID = mLevelLayerIDMap.begin(); 
	 tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {
      int layerID = (*tLevelLayerID).second;
      try {
	Layer& layer = Layer::FindByID( layerID );
	layer.HandleTool( ras.xyz(), mViewState, *this, iTool, iInput );
	if( layer.WantRedisplay() ) {
	  RequestRedisplay();
	  layer.RedisplayPosted();
	}
      }
      catch(...) {
      DebugOutput( << "Couldn't find layer " << layerID );
      }
    }
  }
}

void
ScubaView::DoKeyDown( int iWindow[2], 
		      InputState& iInput, ScubaToolState& iTool ) {
  string key = iInput.Key();

  // Start with a move distance of 1. If we're moving in plane, set
  // that to the specific in plane increment. If control is down,
  // multiplay that value by 10.
  float moveDistance = 1.0;
  if( key == msMoveViewIn || key == msMoveViewOut ) {
    switch( mViewState.mInPlane ) {
    case ViewState::X: 
      moveDistance = mInPlaneMovementIncrements[0]; break;
    case ViewState::Y: 
      moveDistance = mInPlaneMovementIncrements[1]; break;
    case ViewState::Z: 
      moveDistance = mInPlaneMovementIncrements[2]; break;
    }
  }
  if( iInput.IsControlKeyDown() ) {
    moveDistance = 10.0;
  }

  

  if( key == msMoveViewLeft || key == msMoveViewRight ||
      key == msMoveViewDown || key == msMoveViewUp ||
      key == msMoveViewIn   || key == msMoveViewOut ) {
    
    float move[3] = {0, 0, 0};

    if( key == msMoveViewLeft ) {
      switch( mViewState.mInPlane ) {
      case ViewState::X: move[1] = -moveDistance; break;
      case ViewState::Y: move[0] = moveDistance; break;
      case ViewState::Z: move[0] = moveDistance; break;
      }
    } else if( key == msMoveViewRight ) {
      switch( mViewState.mInPlane ) {
      case ViewState::X: move[1] = moveDistance; break;
      case ViewState::Y: move[0] = -moveDistance; break;
      case ViewState::Z: move[0] = -moveDistance; break;
      }
    } else if( key == msMoveViewDown ) {
      switch( mViewState.mInPlane ) {
      case ViewState::X: move[2] = -moveDistance; break;
      case ViewState::Y: move[2] = -moveDistance; break;
      case ViewState::Z: move[1] = -moveDistance; break;
      }
    } else if( key == msMoveViewUp ) {
      switch( mViewState.mInPlane ) {
      case ViewState::X: move[2] = moveDistance; break;
      case ViewState::Y: move[2] = moveDistance; break;
      case ViewState::Z: move[1] = moveDistance; break;
      }
    } else if( key == msMoveViewIn ) {
      switch( mViewState.mInPlane ) {
      case ViewState::X: move[0] = moveDistance; break;
      case ViewState::Y: move[1] = moveDistance; break;
      case ViewState::Z: move[2] = moveDistance; break;
      }
    } else if( key == msMoveViewOut ) {
      switch( mViewState.mInPlane ) {
      case ViewState::X: move[0] = -moveDistance; break;
      case ViewState::Y: move[1] = -moveDistance; break;
      case ViewState::Z: move[2] = -moveDistance; break;
      }
    }

    // Do the move.
    float centerRAS[3];
    float newCenterRAS[3];
    Get2DRASCenter( centerRAS );
    TranslateRASInWindowSpace( centerRAS, move, newCenterRAS );

    Set2DRASCenter( newCenterRAS );

    // Rebuild our label value info because the view has moved.
    float ras[3];
    TranslateWindowToRAS( iWindow, ras );
    RebuildLabelValueInfo( ras, "mouse" );

  } else if( key == msZoomViewIn || key == msZoomViewOut ) {

    float newZoom = mViewState.mZoomLevel;
    if( key == msZoomViewIn ) {
      newZoom = mViewState.mZoomLevel * 2.0;
    } else if( key == msZoomViewOut ) {
      newZoom = mViewState.mZoomLevel / 2.0;
      if( newZoom < 0.25 ) {
	newZoom = 0.25;
      }
    }
    Set2DZoomLevel( newZoom );

    // Rebuild our label value info because the view has moved.
    float ras[3];
    TranslateWindowToRAS( iWindow, ras );
    RebuildLabelValueInfo( ras, "mouse" );
  }

  RequestRedisplay();
}

void
ScubaView::DoKeyUp( int iWindow[2], 
		    InputState& iInput, ScubaToolState& iTool ) {

}

void 
ScubaView::CalcViewToWindowTransform () {

  Point3<float> N( mViewState.mPlaneNormal );
  N = NormalizeVector( N );

  Point3<float> D;
  switch( mViewState.mInPlane ) {
  case ViewState::X: D.Set( 1, 0, 0 ); break;
  case ViewState::Y: D.Set( 0, 1, 0 ); break;
  case ViewState::Z: D.Set( 0, 0, 1 ); break;
  }
  D = NormalizeVector( D );


  double rads = RadsBetweenVectors( N, D );
  if( mViewState.mInPlane == ViewState::X ) {
    rads = -rads;
  }

  Point3<float> axis = CrossVectors( N, D );
  mViewToWindow.MakeRotation( mViewState.mCenterRAS,
			      axis.xyz(), rads );


  CalcWorldToWindowTransform();
}

void 
ScubaView::CalcWorldToWindowTransform () {

  mWorldToWindow = 
    mViewToWindow * mWorldToView->Inverse();

}

void
ScubaView::TranslateWindowToRAS ( int const iWindow[2], float oRAS[3] ) {

  // At zoom level one every pixel is an RAS point, so we're scaled by
  // that and then offset by the RAS center. We find a 3D point by
  // using our mInPlane and the corresponding mCenterRAS in ViewState.

  int xWindow = iWindow[0];
  float windowRAS[3];
  switch( mViewState.mInPlane ) {
  case ViewState::X:
    windowRAS[0] = mViewState.mCenterRAS[0];
    windowRAS[1] = ConvertWindowToRAS( xWindow,
				       mViewState.mCenterRAS[1], mWidth );
    windowRAS[2] = ConvertWindowToRAS(iWindow[1],
				      mViewState.mCenterRAS[2], mHeight );
    break;
  case ViewState::Y:
    if( mbFlipLeftRightInYZ ) {
      xWindow = mWidth - xWindow;
    }
    windowRAS[0] = ConvertWindowToRAS( xWindow,
				       mViewState.mCenterRAS[0], mWidth );
    windowRAS[1] = mViewState.mCenterRAS[1];
    windowRAS[2] = ConvertWindowToRAS( iWindow[1],
				       mViewState.mCenterRAS[2], mHeight );
    break;
  case ViewState::Z:
    if( mbFlipLeftRightInYZ ) {
      xWindow = mWidth - xWindow;
    }
    windowRAS[0] = ConvertWindowToRAS( xWindow,
				       mViewState.mCenterRAS[0], mWidth );
    windowRAS[1] = ConvertWindowToRAS( iWindow[1],
				       mViewState.mCenterRAS[1], mHeight );
    windowRAS[2] = mViewState.mCenterRAS[2];
    break;
  }

  mWorldToWindow.InvMultiplyVector3( windowRAS, oRAS );
}

float
ScubaView::ConvertWindowToRAS ( float iWindow, float iRASCenter, 
				float iWindowDimension ) {

  return ((iWindow - iWindowDimension / 2.0) / mViewState.mZoomLevel) + 
    iRASCenter;
}

void
ScubaView::TranslateRASToWindow ( float const iRAS[3], int oWindow[2] ) {
  
  float windowRAS[3];
  mWorldToWindow.MultiplyVector3( iRAS, windowRAS );

  float xWindow = 0, yWindow = 0;
  switch( mViewState.mInPlane ) {
  case ViewState::X:
    xWindow = ConvertRASToWindow( windowRAS[1],
				  mViewState.mCenterRAS[1], mWidth );
    yWindow = ConvertRASToWindow( windowRAS[2],
				  mViewState.mCenterRAS[2], mHeight );
    break;
  case ViewState::Y:
    xWindow = ConvertRASToWindow( windowRAS[0],
				  mViewState.mCenterRAS[0], mWidth );
    yWindow = ConvertRASToWindow( windowRAS[2],
				  mViewState.mCenterRAS[2], mHeight );
    if( mbFlipLeftRightInYZ ) {
      xWindow = mWidth - xWindow;
    }
    break;
  case ViewState::Z:
    xWindow = ConvertRASToWindow( windowRAS[0],
				  mViewState.mCenterRAS[0], mWidth );
    yWindow = ConvertRASToWindow( windowRAS[1],
				  mViewState.mCenterRAS[1], mHeight );
    if( mbFlipLeftRightInYZ ) {
      xWindow = mWidth - xWindow;
    }
    break;
  }

  oWindow[0] = (int)rint( xWindow );
  oWindow[1] = (int)rint( yWindow );
}

float
ScubaView::ConvertRASToWindow ( float iRAS, float iRASCenter, 
				float iWindowDimension ) {

  return ((iRAS - iRASCenter) * mViewState.mZoomLevel) +
    (iWindowDimension / 2.0);
}

void
ScubaView::TranslateRASInWindowSpace ( float iRAS[3], float iMove[3],
				       float oRAS[3] ) {

  // Translate the view point into a window point.
  Point3<float> window;
  mViewToWindow.MultiplyVector3( iRAS, window.xyz() );

  // Apply the move.
  window[0] += iMove[0];
  window[1] += iMove[1];
  window[2] += iMove[2];
  
  // Convert back into view.
  mViewToWindow.InvMultiplyVector3( window.xyz(), oRAS );
}

void
ScubaView::CalcAllViewIntersectionPoints () {

  list<int> viewIDs;
  GetIDList( viewIDs );
  list<int>::iterator tViewID;
  for( tViewID = viewIDs.begin(); tViewID != viewIDs.end(); ++tViewID ) {
    int viewID = *tViewID;
    if( viewID != GetID () ) {
      CalcViewIntersectionPoints( viewID );
    }
  }
}

void
ScubaView::CalcViewIntersectionPoints ( int iViewID ) {

  View& view = View::FindByID( iViewID );
  // ScubaView& scubaView = dynamic_cast<ScubaView&>(view);
  ScubaView& scubaView = (ScubaView&)view;

  // Get the dot of our plane normal and its plane normal. If not
  // zero...
  Point3<float> n1( mViewState.mPlaneNormal );
  Point3<float> n2;
  scubaView.Get2DPlaneNormal( n2.xyz() );
  if( !AreVectorsParallel( n1, n2 ) ) {
    
    // Get p1 and p2, the center RAS points for our plane
    // and their plane.
    Point3<float> p1( mViewState.mCenterRAS );
    Point3<float> p2;
    scubaView.Get2DRASCenter( p2.xyz() );
    
    // p3 is our RAS point for the edge of the window, and
    // n3 is the normal for the 'side' of the viewing 'box',
    // pointing into the middle of the window. This is
    // definitely perpendicular to n1, but we need to check
    // against n2, because if it is, then we need to change
    // the normal to try a plane in the other
    // orientation. i.e. if we're checking the left side
    // plane, we'll change to the top side plane.
    Point2<int> windowTopLeft( 0, 0 );
    Point3<float> p3;
    TranslateWindowToRAS( windowTopLeft.xy(), p3.xyz() );
    Point3<float> n3;
    switch( Get2DInPlane() ) { 
    case ViewState::X:  n3.Set( 0, 1, 0 ); break;
    case ViewState::Y:  n3.Set( 1, 0, 0 ); break;
    case ViewState::Z:  n3.Set( 1, 0, 0 ); break;
    }
    
    if( AreVectorsParallel( n2, n3 ) ) {
      switch( Get2DInPlane() ) { 
      case ViewState::X:  n3.Set( 0, 0, 1 ); break;
      case ViewState::Y:  n3.Set( 0, 0, 1 ); break;
      case ViewState::Z:  n3.Set( 0, 1, 0 ); break;
      }
    }
    
    // Intersect the three planes. This gives us an RAS
    // interesction.
    Point3<float> P_1((DotVectors(p1,n1) * CrossVectors(n2,n3)) +
		      (DotVectors(p2,n2) * CrossVectors(n3,n1)) +
		      (DotVectors(p3,n3) * CrossVectors(n1,n2)));
    Point3<float> P1 = P_1 / TripleScaleVectors( n1, n2, n3 );
    
    // Now do the right or bottom plane.
    Point2<int> windowBottomRight( mWidth-1, mHeight-1 );
    TranslateWindowToRAS( windowBottomRight.xy(), p3.xyz() );
    
    Point3<float> P_2((DotVectors(p1,n1) * CrossVectors(n2,n3)) +
		      (DotVectors(p2,n2) * CrossVectors(n3,n1)) +
		      (DotVectors(p3,n3) * CrossVectors(n1,n2)));
    Point3<float> P2 = P_2 / TripleScaleVectors( n1, n2, n3 );
 
    // Save the results.
    mViewIDViewIntersectionPointMap[iViewID][0].Set( P1 );
    mViewIDViewIntersectionPointMap[iViewID][1].Set( P2 );
  }
}


void
ScubaView::SetCursor ( float iRAS[3] ) {
  
  // Set the cursor;
  mCursor.Set( iRAS );

  ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
  broadcaster.SendBroadcast( "cursorChanged", NULL );
}

void
ScubaView::SetNextMarker ( float iRAS[3] ) {

  if( mcMarkers > 0 ) {
    
    if( mNextMarker >= mcMarkers )
      mNextMarker = 0;

    mMarkerRAS[mNextMarker] = iRAS;
    mMarkerVisible[mNextMarker] = true;

    mNextMarker++;

    ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
    broadcaster.SendBroadcast( "markerChanged", NULL );
  }
}

void
ScubaView::HideNearestMarker ( float iRAS[3] ) {

  float closestDistance = 10000;
  int nClosestMarker = -1;
  for( int nMarker = 0; nMarker < mcMarkers; nMarker++ ) {
    float distance = 
      sqrt( (mMarkerRAS[nMarker].x() - iRAS[0]) *
	    (mMarkerRAS[nMarker].x() - iRAS[0]) +
	    (mMarkerRAS[nMarker].y() - iRAS[1]) *
	    (mMarkerRAS[nMarker].y() - iRAS[1]) +
	    (mMarkerRAS[nMarker].z() - iRAS[2]) *
	    (mMarkerRAS[nMarker].z() - iRAS[2]) );
    if( distance < closestDistance ) {
      closestDistance = distance;
      nClosestMarker = nMarker;
    }
  }

  if( -1 != nClosestMarker ) {
    mMarkerVisible[nClosestMarker] = false;

    ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
    broadcaster.SendBroadcast( "markerChanged", NULL );
  }
}

void
ScubaView::SetNumberOfMarkers ( int icMarkers ) {
  
  if( icMarkers < mcMarkers ) {
    for( int nMarker = icMarkers; nMarker < mcMarkers; nMarker++ ) {
      mMarkerVisible[nMarker] = false;
    }
  }

  mcMarkers = icMarkers;

  if( mNextMarker >= mcMarkers || mNextMarker < 0 )
    mNextMarker = 0;
}


int
ScubaView::GetFirstUnusedDrawLevel () {

  int highestLevel = 0;
  map<int,int>::iterator tLevelLayerID;
  for( tLevelLayerID = mLevelLayerIDMap.begin(); 
       tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {

    int level = (*tLevelLayerID).first;
    
    highestLevel = level + 1;
  }

  return highestLevel;
}

void
ScubaView::SetFlipLeftRightYZ ( bool iFlip ) {

  mbFlipLeftRightInYZ = iFlip;
  RequestRedisplay();
}


void 
ScubaView::BuildFrameBuffer () {

  // Don't draw if our buffer isn't initialized yet.
  if( NULL == mBuffer )
    return;

  // Erase the frame buffer.
  bzero( mBuffer, mHeight * mWidth * kBytesPerPixel );

  // Go through our draw levels. For each one, get the Layer.
  map<int,int>::iterator tLevelLayerID;
  for( tLevelLayerID = mLevelLayerIDMap.begin(); 
       tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {

    int layerID = (*tLevelLayerID).second;
    try { 
      Layer& layer = Layer::FindByID( layerID );

      // tell it to draw into our buffer with our view state information.
      layer.DrawIntoBuffer( mBuffer, mWidth, mHeight, mViewState, *this );
    }
    catch(...) {
      cerr << "Couldn't find layer " << layerID << endl;
    }
  }
}

void
ScubaView::DrawFrameBuffer () {

#if 0
  glEnable( GL_TEXTURE_2D );
  CheckGLError();

  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 
		mWidth, mHeight, 0,
		GL_RGBA, GL_UNSIGNED_BYTE,
		mBuffer );
  CheckGLError();
  
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
  CheckGLError();
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
  CheckGLError();
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  CheckGLError();
  glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  CheckGLError();
  glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
  CheckGLError();

  glBegin( GL_QUADS );
  CheckGLError();

  glTexCoord2f( 0.0f, 0.0f );
  glVertex3f  ( 0.0f, 0.0f, 0.0f );

  glTexCoord2f( 0.0f, 1.0f );
  glVertex3f  ( 0.0f, (float)mHeight, 0.0f );

  glTexCoord2f( 1.0f, 1.0f );
  glVertex3f  ( (float)mWidth, (float)mHeight, 0.0f );

  glTexCoord2f( 1.0f, 0.0f );
  glVertex3f  ( (float)mWidth, 0.0f, 0.0f );

  glEnd();
  glGetError(); // clear error
#endif  

  glRasterPos2i( 0, 0 );
  glDrawPixels( mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, mBuffer );
}

void 
ScubaView::BuildOverlay () {

  if( !mbRebuildOverlayDrawList )
    return;

  // Open the overlay display list.
  glNewList( kOverlayDrawListID + mID, GL_COMPILE );
    

  // Draw the HUD overlay if necessary. We need to take our edge
  // window coords and translate them to RAS coords and draw them on
  // the sides of the screens. Note w'ere only really using one of the
  // coords in each left/right/bottom/top/plane calculation because we
  // don't care about the other dimensions.
  ScubaGlobalPreferences& prefs = ScubaGlobalPreferences::GetPreferences();
  if( prefs.GetPrefAsBool( ScubaGlobalPreferences::DrawCoordinateOverlay )) {

    int window[2];
    float ras[3];
    char sXLabel, sYLabel, sZLabel;
    float left, right, top, bottom, plane;
    switch( mViewState.mInPlane ) {
    case ViewState::X: 
      sXLabel = 'a';
      sYLabel = 's';
      sZLabel = 'r';
      window[0] = 0;       TranslateWindowToRAS( window, ras ); left  = ras[1];
      window[0] = mWidth;  TranslateWindowToRAS( window, ras ); right = ras[1];
      window[1] = 0;       TranslateWindowToRAS( window, ras ); bottom= ras[2];
      window[1] = mHeight; TranslateWindowToRAS( window, ras ); top   = ras[2];
      plane = ras[0];
      break;
    case ViewState::Y: 
      sXLabel = 'r';
      sYLabel = 's';
      sZLabel = 'a';
      window[0] = 0;       TranslateWindowToRAS( window, ras ); left  = ras[0];
      window[0] = mWidth;  TranslateWindowToRAS( window, ras ); right = ras[0];
      window[1] = 0;       TranslateWindowToRAS( window, ras ); bottom= ras[2];
      window[1] = mHeight; TranslateWindowToRAS( window, ras ); top   = ras[2];
      plane = ras[1];
      break;
    case ViewState::Z: 
    default:
      sXLabel = 'r';
      sYLabel = 'a';
      sZLabel = 's';
      window[0] = 0;       TranslateWindowToRAS( window, ras ); left  = ras[0];
      window[0] = mWidth;  TranslateWindowToRAS( window, ras ); right = ras[0];
      window[1] = 0;       TranslateWindowToRAS( window, ras ); bottom= ras[1];
      window[1] = mHeight; TranslateWindowToRAS( window, ras ); top   = ras[1];
      plane = ras[2];
      break;
    }
    
    glColor3f( 1, 1, 1 );
    
    // Now draw the labels we calc'd before.
    char sLabel[60];
    sprintf( sLabel, "%c%.2f", sXLabel, left );
    glRasterPos2i( 0, mHeight / 2 + 7 );
    for( int nChar = 0; nChar < (int)strlen( sLabel ); nChar++ ) {
      glutBitmapCharacter( GLUT_BITMAP_8_BY_13, sLabel[nChar] );
    }
    sprintf( sLabel, "%c%.2f", sXLabel, right );
    glRasterPos2i( mWidth - strlen(sLabel)*8, mHeight / 2 + 7 );
    for( int nChar = 0; nChar < (int)strlen( sLabel ); nChar++ ) {
      glutBitmapCharacter( GLUT_BITMAP_8_BY_13, sLabel[nChar] );
    }
    sprintf( sLabel, "%c%.2f", sYLabel, top );
    glRasterPos2i( mWidth / 2 - (strlen(sLabel)*8 / 2), mHeight-1-13 );
    for( int nChar = 0; nChar < (int)strlen( sLabel ); nChar++ ) {
      glutBitmapCharacter( GLUT_BITMAP_8_BY_13, sLabel[nChar] );
    }
    sprintf( sLabel, "%c%.2f", sYLabel, bottom );
    glRasterPos2i( mWidth / 2 - (strlen(sLabel)*8 / 2), 4 );
    for( int nChar = 0; nChar < (int)strlen( sLabel ); nChar++ ) {
      glutBitmapCharacter( GLUT_BITMAP_8_BY_13, sLabel[nChar] );
    }
    sprintf( sLabel, "%c%.2f", sZLabel, plane );
    glRasterPos2i( mWidth - (strlen(sLabel)*8), 4 );
    for( int nChar = 0; nChar < (int)strlen( sLabel ); nChar++ ) {
      glutBitmapCharacter( GLUT_BITMAP_8_BY_13, sLabel[nChar] );
    }
    if( mViewState.mZoomLevel != 1 ) {
      sprintf( sLabel, "%.2fx", mViewState.mZoomLevel );
      glRasterPos2i( 0, mHeight-1-13 );
      for( int nChar = 0; nChar < (int)strlen( sLabel ); nChar++ ) {
	glutBitmapCharacter( GLUT_BITMAP_8_BY_13, sLabel[nChar] );
      }
    }
  }

  if( prefs.GetPrefAsBool( ScubaGlobalPreferences::DrawPlaneIntersections )) {

    // Draw our marker color around us.
    glColor3f( mInPlaneMarkerColor[0], 
	       mInPlaneMarkerColor[1], mInPlaneMarkerColor[2] );
    glBegin( GL_LINE_STRIP );
    glVertex2d( 2, 2 );
    glVertex2d( mWidth-3, 2 );
    glVertex2d( mWidth-3, mHeight-3 );
    glVertex2d( 2, mHeight-3 );
    glVertex2d( 2, 2 );
    glEnd();
    

    // For each other visible view...
    list<int> viewIDs;
    GetIDList( viewIDs );
    list<int>::iterator tViewID;
    for( tViewID = viewIDs.begin(); tViewID != viewIDs.end(); ++tViewID ) {

      int viewID = *tViewID;
      
      if( viewID != GetID () ) {

	View& view = View::FindByID( viewID );
	// ScubaView& scubaView = dynamic_cast<ScubaView&>(view);
	ScubaView& scubaView = (ScubaView&)view;

	try {
	  if( scubaView.IsVisibleInFrame() ) {
	    
	    Point3<float> P1, P2;
	    P1.Set( mViewIDViewIntersectionPointMap[viewID][0] );
	    P2.Set( mViewIDViewIntersectionPointMap[viewID][1] );

	    // Transform to window points. These are the two points
	    // to connect to draw a line.
	    Point2<int> drawPoint1;
	    Point2<int> drawPoint2;
	    TranslateRASToWindow( P1.xyz(), drawPoint1.xy() );
	    TranslateRASToWindow( P2.xyz(), drawPoint2.xy() );
	    
	    // Get its marker color.
	    float color[3];
	    scubaView.GetInPlaneMarkerColor( color );

	    // If this is the one we're currently moving, draw it thicker.
	    if( viewID == mCurrentMovingViewIntersection ) {
	      glLineWidth( 3 );
	    } else {
	      glLineWidth( 1 );
	    }	      
	    
	    // Draw the line.
	    glColor3f( color[0], color[1], color[2] );	    
	    glBegin( GL_LINES );
	    glVertex2d( drawPoint1.x(), drawPoint1.y() );
	    glVertex2d( drawPoint2.x(), drawPoint2.y() );
	    glEnd();

	    // Now just get the RAS center and draw a little circle there.
	    Point3<float> centerRAS;
	    Point2<int> centerWindow;
	    scubaView.Get2DRASCenter( centerRAS.xyz() );
	    TranslateRASToWindow( centerRAS.xyz(), centerWindow.xy() );

	    glColor3f( color[0], color[1], color[2] );	    
	    glBegin( GL_LINE_STRIP );
	    glVertex2d( centerWindow.x(), centerWindow.y()-4 );
	    glVertex2d( centerWindow.x()-4, centerWindow.y() );
	    glVertex2d( centerWindow.x(), centerWindow.y()+4 );
	    glVertex2d( centerWindow.x()+4, centerWindow.y() );
	    glVertex2d( centerWindow.x(), centerWindow.y()-4 );
	    glEnd();
	    

	  }
	}
	catch(...) {
	  
	}
      }
    }
  }


  // Draw our markers.
  float range = 1.0;
  switch( mViewState.mInPlane ) {
  case ViewState::X:
    range = mInPlaneMovementIncrements[0] / 2.0;
    break;
  case ViewState::Y:
    range = mInPlaneMovementIncrements[1] / 2.0;
    break;
  case ViewState::Z:
    range = mInPlaneMovementIncrements[2] / 2.0;
    break;
  }

  if( mViewState.IsRASVisibleInPlane( mCursor.xyz(), range ) ) {

    int cursorWindow[2];
    TranslateRASToWindow( mCursor.xyz(), cursorWindow );
    glLineWidth( 1 );
    glColor3f( 1,0,0 );
    glBegin( GL_LINES );
    glVertex2d( cursorWindow[0] - 5, cursorWindow[1] );
    glVertex2d( cursorWindow[0] + 6, cursorWindow[1] );
    glVertex2d( cursorWindow[0], cursorWindow[1] - 5 );
    glVertex2d( cursorWindow[0], cursorWindow[1] + 6 );
    glEnd();
  }

  for( int nMarker = 0; nMarker < mcMarkers; nMarker++ ) {
    if( mMarkerVisible[nMarker] ) {
      int markerWindow[2];
      TranslateRASToWindow( mMarkerRAS[nMarker].xyz(), markerWindow );
      glLineWidth( 1 );
      glColor3f( 0,1,0 );
      glBegin( GL_LINES );
      glVertex2d( markerWindow[0] - 5, markerWindow[1] );
      glVertex2d( markerWindow[0] + 6, markerWindow[1] );
      glVertex2d( markerWindow[0], markerWindow[1] - 5 );
      glVertex2d( markerWindow[0], markerWindow[1] + 6 );
      glEnd();
    }
  }


  glEndList();

  mbRebuildOverlayDrawList = false;
}

void
ScubaView::RebuildLabelValueInfo ( float  iRAS[3],
				   string isLabel) {

  map<string,string> labelValueMap;

  stringstream sID;
  sID << GetLabel() << " (" << GetID() << ")";
  labelValueMap["View"] = sID.str();

  // Get the RAS coords into a string and set that label/value.
  stringstream ssRASCoords;
  char sDigit[10];
  sprintf( sDigit, "%.2f", iRAS[0] );  ssRASCoords << sDigit << " ";
  sprintf( sDigit, "%.2f", iRAS[1] );  ssRASCoords << sDigit << " ";
  sprintf( sDigit, "%.2f", iRAS[2] );  ssRASCoords << sDigit;
  labelValueMap["RAS"] = ssRASCoords.str();

  // Go through our draw levels. For each one, get the Layer.
  map<int,int>::iterator tLevelLayerID;
  for( tLevelLayerID = mLevelLayerIDMap.begin(); 
       tLevelLayerID != mLevelLayerIDMap.end(); ++tLevelLayerID ) {

    int layerID = (*tLevelLayerID).second;
    try {
      Layer& layer = Layer::FindByID( layerID );
      
      // Ask the layer for info strings at this point.
      layer.GetInfoAtRAS( iRAS, labelValueMap );
    }
    catch(...) {
      DebugOutput( << "Couldn't find layer " << layerID );
    }
  }

  // Set this labelValueMap in the array of label values.
  mLabelValueMaps[isLabel] = labelValueMap;

}

void
ScubaView::GetInPlaneMarkerColor ( float oColor[3] ) {

  oColor[0] = mInPlaneMarkerColor[0];
  oColor[1] = mInPlaneMarkerColor[1];
  oColor[2] = mInPlaneMarkerColor[2];
}

void
ScubaView::DrawOverlay () {

  if( mbRebuildOverlayDrawList )
    BuildOverlay();

  glCallList( kOverlayDrawListID + mID );
}

ScubaViewBroadcaster::ScubaViewBroadcaster () {
  mCurrentBroadcaster = -1;
}

View* 
ScubaViewFactory::NewView() { 
  ScubaView* view = new ScubaView();

  // Notify other views of this new one.
  int id = view->GetID();
  ScubaViewBroadcaster& broadcaster = ScubaViewBroadcaster::GetBroadcaster();
  broadcaster.SendBroadcast( "NewView", (void*)&id );

  return view;
}


ScubaViewBroadcaster&
ScubaViewBroadcaster::GetBroadcaster () {
  static ScubaViewBroadcaster* sBroadcaster = NULL;
  if( NULL == sBroadcaster ) {
    sBroadcaster = new ScubaViewBroadcaster();
  }
  return *sBroadcaster;
}

void
ScubaViewBroadcaster::SendBroadcast ( std::string isMessage, void* iData ) {

  // If this is a message for linked views, we want to basically put a
  // mutex on broadcasting.
  if( isMessage == "2DRASCenterChanged" ||
      isMessage == "2DZoomLevelChanged" ||
      isMessage == "2DInPlaneChanged" ) {

    // If -1, no current broadcaster. Save this one's ID and pass the
    // message along. If it is -1, don't pass the broadcast.
    if( mCurrentBroadcaster == -1 ) {
      
      int viewID = *(int*)iData;
      mCurrentBroadcaster = viewID;
      
      Broadcaster::SendBroadcast( isMessage, iData );

      mCurrentBroadcaster = -1;
    }


    // Pass on other kinds of messages.
  } else {
    
    Broadcaster::SendBroadcast( isMessage, iData );
  }
}

ScubaViewStaticTclListener::ScubaViewStaticTclListener () {

  TclCommandManager& commandMgr = TclCommandManager::GetManager();
  commandMgr.AddCommand( *this, "SetNumberOfViewMarkers", 1, "numMarkers", 
			 "Sets the number of view markers." );
  commandMgr.AddCommand( *this, "GetNumberOfViewMarkers", 0, "", 
			 "Returns the number of view markers." );
}

ScubaViewStaticTclListener::~ScubaViewStaticTclListener () {
}

TclCommandListener::TclCommandResult
ScubaViewStaticTclListener::DoListenToTclCommand ( char* isCommand, 
					       int iArgc, char** iasArgv ) {

  // SetNumberOfViewMarkers <number>
  if( 0 == strcmp( isCommand, "SetNumberOfViewMarkers" ) ) {
    int cMarkers = strtol(iasArgv[1], (char**)NULL, 10);
    if( ERANGE == errno ) {
      sResult = "bad number of markers";
      return error;
    }
    
    ScubaView::SetNumberOfMarkers( cMarkers );
  }

  // GetNumberOfViewMarkers
  if( 0 == strcmp( isCommand, "GetNumberOfViewMarkers" ) ) {

      sReturnFormat = "i";
      stringstream ssReturnValues;
      ssReturnValues << ScubaView::GetNumberOfMarkers();
      sReturnValues = ssReturnValues.str();
  }


  return ok;
}



