// 
// Copyright (C) University College London, 2007-2012, all rights reserved.
// 
// This file is part of HemeLB and is CONFIDENTIAL. You may not work 
// with, install, use, duplicate, modify, redistribute or share this
// file, or any part thereof, other than as allowed by any agreement
// specifically made by you with University College London.
// 

#ifndef HEMELBSETUPTOOL_GEOMETRYGENERATOR_H
#define HEMELBSETUPTOOL_GEOMETRYGENERATOR_H

#include <string>
#include <vector>

#include "Iolet.h"
#include "GenerationError.h"

class GeometryWriter;
class Site;
class BlockWriter;
class Block;

class GeometryGenerator {
public:
	GeometryGenerator();
	virtual ~GeometryGenerator();
	void Execute(bool skipNonIntersectingBlocks) throw (GenerationError);
	
	inline std::string GetOutputGeometryFile(void) {
		return this->OutputGeometryFile;
	}
	inline void SetOutputGeometryFile(std::string val) {
		this->OutputGeometryFile = val;
	}

	inline std::vector<Iolet*>& GetIolets() {
		return this->Iolets;
	}
	inline void SetIolets(std::vector<Iolet*> iv) {
		this->Iolets = std::vector<Iolet*>(iv);
	}

	inline void SetOriginWorking(double x, double y, double z) {
		this->OriginWorking[0] = x;
		this->OriginWorking[1] = y;
		this->OriginWorking[2] = z;
	}

	inline void SetSiteCounts(unsigned x, unsigned y, unsigned z) {
		this->SiteCounts[0] = x;
		this->SiteCounts[1] = y;
		this->SiteCounts[2] = z;
	}

	/**
	 * This method implements the algorithm used to approximate the wall normal at a given
	 * fluid site. This is done based on the normal of the triangles intersected by
	 * each lattice link and the distance to those intersections.
	 *
	 * Current implementation does a weighted sum of the wall normals. The weights are the
	 * reciprocal of cut distances along each link.
	 *
	 * @param site Site object with the data required by the algorithm.
	 */
	void ComputeAveragedNormal(Site& site) const;

protected:
	virtual void ComputeBounds(double[]) const = 0;
	virtual void PreExecute(void);
	virtual void ClassifySite(Site& site) = 0;
	//virtual void CreateCGALPolygon(void);
	void WriteSolidSite(BlockWriter& blockWriter, Site& site);
	void WriteFluidSite(BlockWriter& blockWriter, Site& site);
	// Members set from outside to initialise
	double OriginWorking[3];
	unsigned SiteCounts[3];
	std::string OutputGeometryFile;
	std::vector<Iolet*> Iolets;
	virtual int BlockInsideOrOutsideSurface(const Block &block) = 0;
};

#endif // HEMELBSETUPTOOL_GEOMETRYGENERATOR_H
