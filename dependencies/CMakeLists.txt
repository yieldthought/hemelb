cmake_minimum_required (VERSION 2.8)
Project(HemeLBDependencies)
include(ExternalProject)
set(DEPENDENCIES_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(DEPENDENCIES_INSTALL_PATH ${DEPENDENCIES_PATH} CACHE STRING "Path to install Dependencies")
set(CMAKE_MODULE_PATH "${DEPENDENCIES_PATH}/Modules/" ${CMAKE_MODULE_PATH})

find_package(MPI REQUIRED) # Do this *after* we include the patched find MPI module

# --------- tinyxml -------------------
find_package(TinyXML QUIET)
if (TINYXML_FOUND)
	message("TinyXML already installed, no need to download")
else()
	message("TinyXML not installed, will download and build")
	OPTION(TIXML_USE_STL "Use STL with TIXML" ON)
	if(TIXML_USE_STL)
		add_definitions(-DTIXML_USE_STL)
	endif()
	find_file(TINYXML_TARBALL tinyxml_2_6_2.tar.gz 
		DOC "Path to download TinyXML (can be url http://)"
		PATHS ${DEPENDENCIES_PATH}/distributions
	)
	if(NOT TINYXML_TARBALL)
		set(TINYXML_TARBALL http://sourceforge.net/projects/tinyxml/files/tinyxml/2.6.2/tinyxml_2_6_2.tar.gz
			CACHE STRING "Path to download TinyXML (can be local file://)" FORCE)
	endif()
	ExternalProject_Add(
	   tinyxml
		INSTALL_DIR ${DEPENDENCIES_INSTALL_PATH}
		URL ${TINYXML_TARBALL}
		CONFIGURE_COMMAND cmake <SOURCE_DIR> 
		-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
		-DTIXML_USE_STL=${TIXML_USE_STL}
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_FLAGS_RELEASE=-O4
		PATCH_COMMAND cp ${DEPENDENCIES_PATH}/patches/tinyxml.cmake CMakeLists.txt
	)
endif()

# ------- parmetis --------------------
find_package(Parmetis QUIET)
if (PARMETIS_FOUND)
	message("Parmetis already installed, no need to download")
else()
	message("Parmetis not installed, will download and build")
	find_file(PARMETIS_TARBALL parmetis-4.0.2.tar.gz 
		DOC "Path to download Parmetis (can be url http://)"
		PATHS ${DEPENDENCIES_PATH}/distributions
	)
	if(NOT PARMETIS_TARBALL)
		set(PARMETIS_TARBALL http://glaros.dtc.umn.edu/gkhome/fetch/sw/parmetis/parmetis-4.0.2.tar.gz 
		CACHE STRING "Path to download Parmetis (can be local file://)" FORCE)
	endif()
	set(PARMETIS_CC ${MPI_C_COMPILER} CACHE STRING "MPI Compiler to use for Parmetis, leave blank to let parmetis guess")
	set(PARMETIS_CXX ${MPI_CXX_COMPILER} CACHE STRING "MPI Compiler to use for Parmetis, leave blank to let parmetis guess")
	if(PARMETIS_CC)
		set(PARMETIS_CC_OPTION cc=${PARMETIS_CC})
	endif()
	if(PARMETIS_CXX)
		set(PARMETIS_CXX_OPTION cxx=${PARMETIS_CXX})
	endif()
	ExternalProject_Add(
	   parmetis
		INSTALL_DIR ${DEPENDENCIES_INSTALL_PATH}
		URL ${PARMETIS_TARBALL}
	   CONFIGURE_COMMAND make config prefix=<INSTALL_DIR> ${PARMETIS_CC_OPTION} ${PARMETIS_CXX_OPTION} && 
				cd metis && 
				make config prefix=<INSTALL_DIR> ${PARMETIS_CC_OPTION} ${PARMETIS_CXX_OPTION}
		BUILD_IN_SOURCE 1
		BUILD_COMMAND make MAKEFLAGS= #Workaround for bug in parmetis makefile treating switches as targets
		INSTALL_COMMAND make install MAKEFLAGS= && cd metis && make install MAKEFLAGS=
	)
endif()

# -------CPPUNIT -------------
find_package(CPPUnit QUIET)
if (CPPUNIT_FOUND)
	message("CPPUnit already installed, no need to download")
else()
	message("CPPUnit not installed, will download and build")
	find_file(CPPUNIT_TARBALL cppunit-1.12.1.tar.gz 
		DOC "Path to download CPPUNIT (can be url http://)"
		PATHS ${DEPENDENCIES_PATH}/distributions
	)
	if(NOT CPPUNIT_TARBALL)
		set(CPPUNIT_TARBALL http://downloads.sourceforge.net/cppunit/cppunit-1.12.1.tar.gz
			CACHE STRING "Path to download CPPUNIT (can be local file://)" FORCE)
	endif()
	if(APPLE)
		set(RECONFIGURE_CPPUNIT_DEFAULT ON)
	else()
		set(RECONFIGURE_CPPUNIT_DEFAULT OFF)
	endif()
	option(RECONFIGURE_CPPUNIT "Include Reconfigure step for CPPUNIT" ${RECONFIGURE_CPPUNIT_DEFAULT})
	ExternalProject_Add(
	   cppunit
		INSTALL_DIR ${DEPENDENCIES_INSTALL_PATH}
		URL ${CPPUNIT_TARBALL}
	   CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --mandir=<INSTALL_DIR>/share/man --disable-doxygen --disable-dot
		BUILD_IN_SOURCE 1
	)
	if (RECONFIGURE_CPPUNIT)
		ExternalProject_Add_Step(
			cppunit reconf
			COMMAND autoreconf -fvi #autoreconf required on osx - based on contents of portfile
			DEPENDEES download
			DEPENDERS configure
			WORKING_DIRECTORY <SOURCE_DIR>
		)
	endif()
endif()

# ---------BOOST -----------------------
set(Boost_ADDITIONAL_VERSIONS "1.48" "1.48.0")
set(BOOST_LIBS mpi regex)
string(REPLACE ";" "," BOOST_LIBS_COMMA "${BOOST_LIBS}")
find_package(Boost 1.48 COMPONENTS ${BOOST_LIBS})
if (BOOST_FOUND)
	message("Boost 1.48 already installed, no need to download")
else()
	message("Bost not installed, will download and build")
	find_file(BOOST_TARBALL boost_1_48_0.tar.gz
		DOC "Path to download BOOST (can be url http://)"
		PATHS ${DEPENDENCIES_PATH}/distributions
	)
	if(NOT BOOST_TARBALL)
		set(BOOST_TARBALL http://sourceforge.net/projects/boost/files/boost/1.48.0/boost_1_48_0.tar.gz
			CACHE STRING "Path to download BOOST (can be local file://)" FORCE)
	endif()
	ExternalProject_Add(
   	boost
		INSTALL_DIR ${DEPENDENCIES_INSTALL_PATH}
		URL ${BOOST_TARBALL}
		BUILD_COMMAND <SOURCE_DIR>/b2
		INSTALL_COMMAND <SOURCE_DIR>/b2 install
		CONFIGURE_COMMAND <SOURCE_DIR>/bootstrap.sh --prefix=<INSTALL_DIR> "--with-libraries=${BOOST_LIBS_COMMA}"
		BUILD_IN_SOURCE 1
	)
	ExternalProject_Add_Step(
		boost mpi
		COMMAND cat ${DEPENDENCIES_PATH}/patches/boost_options.jam >> project-config.jam
		DEPENDEES configure
		DEPENDERS build
		WORKING_DIRECTORY <SOURCE_DIR>
	)
endif()

#----------------Google C++ template ----------------
find_package(CTemplate)
if (CTEMPLATE_FOUND)
	message("Ctemplate already installed, no need to download")
else()
	message("Ctemplate not installed, will download and build")
	find_file(CTEMPLATE_TARBALL ctemplate-1.0.tar.gz
		DOC "Path to download CTemplate (can be url http://)"
		PATHS ${DEPENDENCIES_PATH}/distributions
	)
	if(NOT CTEMPLATE_TARBALL)
		set(CTEMPLATE_TARBALL http://google-ctemplate.googlecode.com/files/ctemplate-1.0.tar.gz
			CACHE STRING "Path to download CTemplate (can be local file://)" FORCE)
	endif()
	ExternalProject_Add(
   	ctemplate
		INSTALL_DIR ${DEPENDENCIES_INSTALL_PATH}
		URL ${CTEMPLATE_TARBALL}
	   CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
		BUILD_IN_SOURCE 1
	)
endif()