add_library(
	hemelb_geometry BlockTraverser.cc BlockTraverserWithVisitedBlockTracker.cc 
	GeometryReader.cc needs/Needs.cc LatticeData.cc SiteData.cc SiteType.cc 
	SiteTraverser.cc VolumeTraverser.cc Block.cc 
	decomposition/BasicDecomposition.cc decomposition/OptimisedDecomposition.cc
	neighbouring/NeighbouringLatticeData.cc	neighbouring/NeighbouringDataManager.cc
	neighbouring/RequiredSiteInformation.cc
	)
configure_file (
  "${PROJECT_SOURCE_DIR}/geometry/decomposition/DecompositionWeights.h.in"
  "${PROJECT_BINARY_DIR}/geometry/decomposition/DecompositionWeights.h"
  )

