// ----------------------------------------------------------------------------
// engine_helper.h : common structure and class serving the SVL engine
//
// Creation : Mar. 23rd 2011
// revision:
//
// Copyright(C) 2012-2013 Haipeng Cai
//
// ----------------------------------------------------------------------------
#ifndef _ENGINE_HELPER_H_
#define _ENGINE_HELPER_H_

#include <vtkSmartPointer.h>
#include <vtkBoxWidget.h>
#include <vtkVolume.h>
#include <vtkVolumeMapper.h>
#include <vtkTubeFilter.h>
#include <vtkPolyData.h>
#include <vtkCommand.h>

#include <vtkCamera.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkIndent.h>

#include <vtkPlanes.h>

#include <vtkPlaneWidget.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>

#include "vtkThresholdedPolyDataFilter.h"

#include "vtkTubeHaloMapper.h" 
#include "vtkTubeFilterEx.h"
#include "vtkDepthSortPoints.h"

#include "point.h"

#include <string>
#include <iostream>
#include <vector>
#include <map>

#define WINDOW_TITLE "Zifazah 1.1.0"

class imgVolRender;

typedef struct _depth_color_t {
	vector_t hue;
	vector_t satu;
	vector_t value;

	_depth_color_t() :
		hue(0.0,0.0),
		satu(0.0,0.0),
		value(0.0,1.0){
	}
} depth_color_t;

typedef struct _depth_size_t {
	double size;
	double scale;

	_depth_size_t() {
		size = 0.03;
		scale = 20;
	}
} depth_size_t;

/* diffferent rendering focus */
enum {
	FOCUS_VOL = 0,
	FOCUS_MVOL,
	FOCUS_GEO,
	FOCUS_COMPOSITE
};

/* depth mappings */
enum {
	VN_NONE = -1,
	VN_SIZE = 0,
	VN_COLOR,
	VN_TRANSPARENCY,
	VN_VALUE,
	VN_HALO
};

/* debug levels */
enum {
	DL_NONE = -1,
	DL_LOG,
	DL_WARNING,
	DL_FATAL,
	DL_EVAL
};

/* shape representations */
enum {
	SHAPE_NONE = -1,
	SHAPE_LINE = 0,
	SHAPE_TUBE,
	SHAPE_RIBBON
};

typedef struct _mixed_pipeline {

	vtkSmartPointer<vtkActor> m_actor;

	vtkSmartPointer<vtkThresholdedPolyDataFilter>	m_fiberSelector;
	vtkSmartPointer<vtkThresholdedPolyDataFilter>	m_faSelector;

	bool	m_bDepthSize;
	bool	m_bDepthTransparency;
	bool	m_bDepthColor;
	bool	m_bDepthColorLAB;
	bool	m_bDepthValue;
	bool	m_bSizeByFA;

	vector_t	m_transparency;
	depth_color_t m_dptcolor;
	depth_size_t m_dptsize;
	vector_t	m_value;

	vtkDepthSortPoints* m_dsort;
	vtkDepthSortPoints* m_linesort;
	vtkTubeFilter*	m_streamtube;

	int	m_shape;

	bool	m_bInit;

} mixed_pipeline;

// Callback for moving the planes from the box widget to the mapper
class vtkBoxWidgetCallback : public vtkCommand
{
public:
  static vtkBoxWidgetCallback *New();
  virtual void Execute(vtkObject *caller, unsigned long, void*);
  void SetMapper(vtkVolumeMapper* m);

protected:
  vtkBoxWidgetCallback();

private:
  vtkVolumeMapper *Mapper;
};

// Callback for moving the planes from the plane widget to the mapper
class VTK_COMMON_EXPORT vtkPlaneWidgetCallback : public vtkCommand
{
public:
  vtkTypeMacro(vtkPlaneWidgetCallback,vtkCommand);

  static vtkPlaneWidgetCallback *New();
  virtual void Execute(vtkObject *caller, unsigned long, void*);
  void SetMapper(vtkMapper* m);
  void SetPipelines(std::vector< mixed_pipeline >* p);

  void SetIndex(int i) { Index = i; }
  int GetIndex() const { return Index; }

protected:
  vtkPlaneWidgetCallback();

private:
  vtkMapper *Mapper;
  std::vector< mixed_pipeline >* pipelines;
  int	Index;
};

// Callback for keyPressEvent handling emitted from the vtk rendering view
class vtkViewKeyEventCallback: public vtkCommand
{
public:
  static vtkViewKeyEventCallback *New();
  virtual void Execute(vtkObject *caller, unsigned long, void*);

protected:
  vtkViewKeyEventCallback();

private:
};

#endif // _ENGINE_HELPER_H_

/* sts=8 ts=8 sw=80 tw=8 */

