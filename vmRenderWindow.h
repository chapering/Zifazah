// ----------------------------------------------------------------------------
// vmRenderWindow.h : volume rendering Window with Qt Mainwindow
//
// Creation : Nov. 12th 2011
// revision:
//	- this class has been extensively expanded to serve visualization language - 
//		March 2012
//
// Copyright(C) 2011-2012 Haipeng Cai
//
// ----------------------------------------------------------------------------
#ifndef _VMRENDERWINDOW_H_
#define _VMRENDERWINDOW_H_

#include <QMainWindow>
#include <QSize>
#include <QSizePolicy>
#include <QListWidget>
#include <QFile>

#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkBoxWidget.h>
#include <vtkVolume.h>
#include <vtkVolumeCollection.h>
#include <vtkVolumeMapper.h>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkTubeFilter.h>
#include <vtkPolyData.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkInteractorStyle.h>

#include <vtkDepthSortPolyData.h>
#include <vtkCamera.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkIndent.h>

#include <vtkPlanes.h>

#include <vtkTextureMapToCylinder.h>
#include <vtkTextureMapToSphere.h>
#include <vtkTextureMapToPlane.h>
#include <vtkPNGReader.h>
#include <vtkTexture.h>
#include <vtkTransformTextureCoords.h>
#include <vtkPolyDataReader.h>
#include <vtkRibbonFilter.h>

#include <vtkOutlineFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkContourFilter.h>

#include <QVTKInteractor.h>

#include <vtkScalarBarActor.h>
#include <vtkTextProperty.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkAnnotatedCubeActor.h>
#include <vtkAppendPolyData.h>
#include <vtkAxesActor.h>
#include <vtkPropAssembly.h>
#include <vtkPropCollection.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkCaptionActor2D.h>
#include <vtkAppendPolyData.h>
#include <vtkMath.h>
#include <vtkRectangularButtonSource.h>
#include <vtkCubeSource.h>
#include <vtkProperty2D.h>

#include <vtkPlaneWidget.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>

#include <vtkThreshold.h>
#include "vtkThresholdedPolyDataFilter.h"

#include "vtkTubeHaloMapper.h" 
#include "vtkTubeFilterEx.h"
#include "vtkDepthSortPoints.h"
#include "vtkLegiInteractionRecorder.h"
#include "ui_testbed.h"
#include "imgVolRender.h"

#include "tgdataReader.h"
#include "cppmoth.h"
#include "point.h"
#include "Gadgets.h"

#include "vtkMPIController.h"
#include "vtkMultiProcessController.h"
#include "vtkLegiRenderManager.h"

#include <string>
#include <iostream>
#include <vector>
#include <map>
#define _DEFINE_DEPRECATED_HASH_CLASSES 0

class imgVolRender;

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

  void SetIndex(int i) { Index = i; }
  int GetIndex() const { return Index; }

protected:
  vtkPlaneWidgetCallback();

private:
  vtkMapper *Mapper;
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

void process(vtkMultiProcessController* controller, void* arg);
void UpdateMapping(void *arg, void *, int, int);

class CLegiMainWindow : public QMainWindow, public Ui::TestbedWindow, public CApplication
{
	Q_OBJECT
public:
	friend class imgVolRender;
	friend void process(vtkMultiProcessController* controller, void* arg);

	friend void UpdateMapping(void *arg, void *, int, int);

	CLegiMainWindow(int argc, char** argv, vtkMultiProcessController* controller, QWidget* parent = 0, Qt::WindowFlags f = 0);
	virtual ~CLegiMainWindow();
	int handleOptions(int optv);
	int mainstay();

	void keyPressEvent (QKeyEvent* event);
	void resizeEvent( QResizeEvent* event );

	void draw();
	void show();

	QVTKWidget*& getRenderView();

	void addBoxWidget();

	MyCout& getcout() { return m_cout; }

protected:
	/* source file holding helper info*/
	string m_strfnhelp;

	/* text file holding a list of tasks for a single session */
	string m_strfntask;

	string m_strfnskeleton;

	/* number of selection box */
	int	m_nselbox;
	int m_lod;
	double m_fRadius;

signals:
	void close();

public slots:
	void onactionLoadVolume();
	void onactionLoadGeometry();
	void onactionLoadData();
	void onactionClose_current();

	void onactionVolumeRender();
	void onactionGeometryRender();
	void onactionMultipleVolumesRender();
	void onactionCompositeRender();
	void onactionTF_customize();
	void onactionSettings();

	void onKeys(vtkObject* obj,unsigned long,void* client_data,void*,vtkCommand* command);

	/* settings .... */
	void onVolRenderMethodChanged(int index);
	void onVolRenderPresetChanged(int index);

	void onHaloStateChanged(int state);

	void onTubeSizeStateChanged(int state);
	void onTubeSizeChanged(double d);
	void onTubeSizeScaleChanged(double d);

	void onTubeAlphaStateChanged(int state);
	void onTubeAlphaStartChanged(double d);
	void onTubeAlphaEndChanged(double d);

	void onTubeColorStateChanged(int state);
	void onTubeHueStartChanged(double d);
	void onTubeHueEndChanged(double d);
	void onTubeSatuStartChanged(double d);
	void onTubeSatuEndChanged(double d);
	void onTubeValueStartChanged(double d);
	void onTubeValueEndChanged(double d);

	void onTubeDValueStateChanged(int state);
	void onTubeDValueStartChanged(double d);
	void onTubeDValueEndChanged(double d);

	void onTubeColorLABStateChanged(int state);

	void onHaloWidthChanged(int i);

	void onHatchingStateChanged(int state);


	void onButtonApply();

	/* scripting .... */
	void onactionLoadScript();
	void onactionSaveScript();
	void onactionRunScript();
	void onactionScripting();
	void onactionClearOutput();

	void onButtonRun();

	int onTaskFinished() { return 1; }
private:
	imgVolRender* m_pImgRender;
	vtkTgDataReader* m_pstlineModel;
	vtkSmartPointer<vtkRenderer> m_render;
	vtkEventQtSlotConnect*	qvtkConnections;
	vtkBoxWidget* m_boxWidget;
	vtkSmartPointer<vtkCamera> m_camera;
	vtkSmartPointer<vtkActor> m_actor;
	vtkSmartPointer<vtkActor> m_haloactor;
	vtkSmartPointer<vtkScalarBarActor> m_colorbar;
	vtkSmartPointer<vtkOrientationMarkerWidget> m_orientationWidget;
	vtkSmartPointer<vtkLegiInteractionRecorder> m_recorder;

	vtkSmartPointer<vtkThresholdedPolyDataFilter>	m_fiberSelector;
	vtkSmartPointer<vtkThresholdedPolyDataFilter>	m_faSelector;

	QSize	m_oldsz;

	std::string	m_fnData;
	std::string m_fnVolume;
	std::string m_fnGeometry;

	int		m_nHaloType;
	int		m_nHaloWidth;

	bool	m_bDepthHalo;
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

	int	m_nCurMethodIdx;
	int m_nCurPresetIdx;

	bool	m_bLighting;
	bool	m_bCapping;
	bool	m_bHatching;

	int		m_curFocus;
	bool	m_bInit;
	bool	m_bFirstHalo;

	bool	m_bCurveMapping;
	vtkSmartPointer<vtkPoints>	m_labColors;
	vtkDepthSortPoints* m_dsort;
	vtkDepthSortPoints* m_linesort;
	vtkTubeFilter*	m_streamtube;
	vtkMultiProcessController* m_controller;

	vtkSmartPointer<CVTKTextbox> m_taskbox;
	vtkSmartPointer<CVTKTextbox> m_helptext;

	std::vector< vtkCubeSource* >	m_boxes;
	std::vector< std::string >		m_strfnBoxes;
	/* store variable for language reference 
	 * simple mapping: variable name -> right expression as a literal string
	 */
	std::map< std::string, std::string > m_strVarTable;
	vtkSmartPointer<vtkPlaneWidget>	m_planeX;
	vtkSmartPointer<vtkPlaneWidget>	m_planeY;
	vtkSmartPointer<vtkPlaneWidget>	m_planeZ;

	int		m_nKey;
	int		m_nFlip;

	double	m_wholeExtent[6];
private:
	/**
	 *@param type - indicate how to render geometries 
	 *@note 
	 *	0 - no halo
	 *	1 - consistent halo
	 *	2 - depth dependent halo
	 */
	void __renderTubes(int type=0);
	void __renderLines(int type=0);
	void __renderRibbons();
	void __depth_dependent_size();
	void __depth_dependent_transparency();
	void __depth_dependent_halos();

	void __renderDepthSortTubes();

	void __addHaloType1();

	void __uniformTubeRendering();
	void __uniform_halos();

	void __removeAllActors();
	void __removeAllVolumes();

private:
	void __init_volrender_methods();
	void __init_tf_presets();

	void __add_texture_strokes();
	void __iso_surface(bool);
	void __add_axes();
	void __add_planes();
	int __loadBoxpos();

	double _calBlockAvgFA(double* boxExtent, int & nLineInbox);

	void __load_colormap();

	void selfRotate(int range);

	/* all following private routines are dedicated to serve a simple script interpreter */

	// execute a single line of the script
	int __execLine(const QString&  curline);

	// simply map literal fiber bundle name to an index in the static array of known bundles
	int __getFiberBundleIdx(const QString& bundleName);

	/* serve the four different type of usage scenarios respectively */
	int __scenario_explore(const QString&  curline);
	int __scenario_generate(const QString&  curline);
	int __scenario_compare(const QString&  curline);
	int __scenario_measure(const QString&  curline);

	/* parse a conditional or arithmetical expression */
	int __parseCondExp(const QString& exp);

	/* parse a parameter list for the update statement */
	int __parseParaList(const QString& exp, int curTag);

	/* debugging information output */
	void __setDebugLevel(int debugLevel);
	void __debugOutput(const QString& msg, int debugLevel=DL_NONE);

	int __spatialOps(const QString& sop);
};

#endif // _VMRENDERWINDOW_H_

/* sts=8 ts=8 sw=80 tw=8 */

