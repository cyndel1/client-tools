
#ifndef _MSelectionMask
#define _MSelectionMask
//
//-
// ==========================================================================
// Copyright (C) 1995 - 2006 Autodesk, Inc., and/or its licensors.  All 
// rights reserved.
// 
// The coded instructions, statements, computer programs, and/or related 
// material (collectively the "Data") in these files contain unpublished 
// information proprietary to Autodesk, Inc. ("Autodesk") and/or its 
// licensors,  which is protected by U.S. and Canadian federal copyright law 
// and by international treaties.
// 
// The Data may not be disclosed or distributed to third parties or be 
// copied or duplicated, in whole or in part, without the prior written 
// consent of Autodesk.
// 
// The copyright notices in the Software and this entire statement, 
// including the above license grant, this restriction and the following 
// disclaimer, must be included in all copies of the Software, in whole 
// or in part, and all derivative works of the Software, unless such copies 
// or derivative works are solely in the form of machine-executable object 
// code generated by a source language processor.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND. 
// AUTODESK DOES NOT MAKE AND HEREBY DISCLAIMS ANY EXPRESS OR IMPLIED 
// WARRANTIES INCLUDING, BUT NOT LIMITED TO, THE WARRANTIES OF 
// NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, 
// OR ARISING FROM A COURSE OF DEALING, USAGE, OR TRADE PRACTICE. IN NO 
// EVENT WILL AUTODESK AND/OR ITS LICENSORS BE LIABLE FOR ANY LOST 
// REVENUES, DATA, OR PROFITS, OR SPECIAL, DIRECT, INDIRECT, OR 
// CONSEQUENTIAL DAMAGES, EVEN IF AUTODESK AND/OR ITS LICENSORS HAS 
// BEEN ADVISED OF THE POSSIBILITY OR PROBABILITY OF SUCH DAMAGES. 
// ==========================================================================
//+
//
// CLASS:    MSelectionMask
//
// *****************************************************************************
//
// CLASS DESCRIPTION (MSelectionMask)
//
// Selection masks provide a way to control what is selectable in Maya.
// 
// Selection masks can be created by OR'ing several types together, or
// by using the addMask method to add a selection type to an MSelectionMask.
//
// *****************************************************************************

#if defined __cplusplus

// *****************************************************************************

// INCLUDED HEADER FILES



#include <maya/MObject.h>
#include <maya/MStatus.h>

// *****************************************************************************

// DECLARATIONS

class MPoint;
class MString;
class MTime;

// *****************************************************************************

// CLASS DECLARATION (MSelectionMask)

/// Manage what is selectable in Maya
/**
  Selection masks provide a way to control what is selectable in Maya.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MSelectionMask
{
public:
	///
	enum SelectionType {
		///
		kSelectHandles,
		///
		kSelectLocalAxis,
		
		///
		kSelectIkHandles,
		///
		kSelectIkEndEffectors,
		///
		kSelectJoints,
		
		///
		kSelectLights,
		///
		kSelectCameras,
		
		///
		kSelectLattices,
		///
		kSelectClusters,
		///
		kSelectSculpts,
		
		///
		kSelectNurbsCurves,
		///
		kSelectNurbsSurfaces,
		///
		kSelectMeshes,
		///
		kSelectSubdiv,
		///
		kSelectSketchPlanes,
		
		///
		kSelectParticleShapes,
		///
		kSelectEmitters,
		///
		kSelectFields,
		///
		kSelectSprings,
		///
		kSelectRigidBodies,
		///
		kSelectRigidConstraints,
		///
		kSelectCollisionModels,
		
		///
		kSelectXYZLocators,
		///
		kSelectOrientationLocators,
		///
		kSelectUVLocators,
		
		///
		kSelectTextures,

		///
		kSelectCurves,
		///
		kSelectSurfaces,
		///
		kSelectLocators,
		///
		kSelectObjectsMask,
		
		
		///
		kSelectCVs,
		///
		kSelectHulls,
		///
		kSelectEditPoints,
		
		///
		kSelectMeshVerts,
		///
		kSelectMeshEdges,
		///
		kSelectMeshFreeEdges,
		///
		kSelectMeshFaces,
		///
		kSelectSubdivMeshPoints,
		///
		kSelectSubdivMeshEdges,
		///
		kSelectSubdivMeshFaces,
		///
		kSelectMeshUVs,
		
		///
		kSelectVertices,
		///
		kSelectEdges,
		///
		kSelectFacets,
		///
		kSelectMeshLines,
		///
		kSelectMeshComponents,
		
		///
		kSelectCurveParmPoints,
		///
		kSelectCurveKnots,
		///
		kSelectSurfaceParmPoints,
		///
		kSelectSurfaceKnots,
		///
		kSelectSurfaceRange,
		///
		kSelectSurfaceEdge,
		///
		kSelectIsoparms,
		///
		kSelectCurvesOnSurfaces,
		///
		kSelectPPStrokes,
		
		///
		kSelectLatticePoints,
		
		///
		kSelectParticles,
		///

		kSelectJointPivots,
		///
		kSelectScalePivots,
		///
		kSelectRotatePivots,
		
		///
		kSelectPivots,
		
		///
		kSelectSelectHandles,
		
		///
		kSelectComponentsMask,
		
		///
		kSelectAnimCurves,
		///
		kSelectAnimKeyframes,
		///
		kSelectAnimInTangents,
		///
		kSelectAnimOutTangents,
		
		///
		kSelectAnimMask,
		///
		kSelectAnimAny,
		
		///
		kSelectTemplates,

		///
		kSelectManipulators,
		///
		kSelectGuideLines,
		///
		kSelectPointsForGravity,
		///
		kSelectPointsOnCurvesForGravity,
		///
		kSelectPointsOnSurfacesForGravity,
		///
		kSelectObjectGroups,
		///
		kSelectSubdivMeshMaps,
		///
		kSelectFluids,
		///
		kSelectHairSystems,
		///
		kSelectFollicles
	};

	///
	MSelectionMask();
	///
	MSelectionMask( SelectionType selType );
	///
	MSelectionMask( const MSelectionMask& in );

	///
	virtual ~MSelectionMask();

	///
	MStatus		setMask( SelectionType selType );
	///
	MStatus		setMask( MSelectionMask& mask );
	///
	MStatus		addMask( SelectionType selType );

	///
	bool		intersects( SelectionType selType,
							MStatus * ReturnStatus = NULL ) const;
	///
	bool		intersects( MSelectionMask& mask,
							MStatus * ReturnStatus = NULL ) const;

	///
	MSelectionMask	operator| ( SelectionType selType );
	///
	MSelectionMask	operator| ( MSelectionMask& mask );
	///
	MSelectionMask&	operator =( MSelectionMask& other );

protected:
// No protected members

private:
	MSelectionMask( const void *, bool );
	const void * data;
	bool fOwn;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

// *****************************************************************************
#endif /* __cplusplus */
#endif /* _MSelectionMask */