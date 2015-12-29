// ----------------------------------------------------------------------------
// vmRenderWindow.cpp : volume rendering Window with Qt Mainwindow
//
// Creation : Nov. 12th 2011
//
// Copyright(C) 2011-2012 Haipeng Cai
//
// ----------------------------------------------------------------------------
#include "vmRenderWindow.h"
#include "vtkLegiInteractorStyle.h"
#include "vtkPolyDataMapper2D.h"
#include "vtkLegiProperty.h"
#include "vtkLegiPolyDataMapper.h"
#include "vtkCanvasWidget.h"

#include <vtkPolyLine.h>
#include <vtkPolygon.h>
#include <vtkLine.h>
#include <vtkMatrix4x4.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkActor2D.h>
#include <vtkProperty2D.h>
#include <vtkCoordinate.h>
#include <vtkLight.h>

#include <vtkGeometryFilter.h>
#include <vtkPolyDataAlgorithm.h>

#include <vtkProbeFilter.h>

#include <QXmlStreamReader>
#include <QFile>
#include <QStringList>
#include <QRegExp>

using namespace std;

#define TUBE_LOD 5
/* extend RMI messages */
#define RENDER_RMI_CAMERA_UPDATE 87840
#define RENDER_RMI_MAPPING_UPDATE 87841
#define RENDER_RMI_REDRAW 87842

//////////////////////////////////////////////////////////////////////
// implementation of class vtkBoxWidgetCallback 
//////////////////////////////////////////////////////////////////////
vtkBoxWidgetCallback::vtkBoxWidgetCallback()
{ 
	this->Mapper = 0; 
}

vtkBoxWidgetCallback* vtkBoxWidgetCallback::New()
{
	return new vtkBoxWidgetCallback; 
}

void vtkBoxWidgetCallback::Execute(vtkObject *caller, unsigned long, void*)
{
	vtkBoxWidget *widget = reinterpret_cast<vtkBoxWidget*>(caller);
	if (this->Mapper)
	{
		vtkPlanes *planes = vtkPlanes::New();
		widget->GetPlanes(planes);
		this->Mapper->SetClippingPlanes(planes);
		planes->Delete();
	}
}

void vtkBoxWidgetCallback::SetMapper(vtkVolumeMapper* m) 
{ 
	this->Mapper = m; 
}

//////////////////////////////////////////////////////////////////////
// implementation of class vtkPlaneWidgetCallback 
//////////////////////////////////////////////////////////////////////
vtkPlaneWidgetCallback::vtkPlaneWidgetCallback()
{
	this->Mapper = 0; 
	this->Index = 0;
}

vtkPlaneWidgetCallback* vtkPlaneWidgetCallback::New()
{
	return new vtkPlaneWidgetCallback; 
}

void vtkPlaneWidgetCallback::Execute(vtkObject *caller, unsigned long, void*)
{
	vtkPlaneWidget *widget = reinterpret_cast<vtkPlaneWidget*>(caller);
	if (this->Mapper)
	{
		vtkPlane *plane = vtkPlane::New();
		widget->GetPlane(plane);
		/*
		this->Mapper->RemoveClippingPlane( this->Mapper->GetClippingPlanes()->GetItem( this->GetIndex() ) );
		this->Mapper->AddClippingPlane(plane);
		*/
		if ( this->Mapper->GetClippingPlanes() && 
				this->Mapper->GetClippingPlanes()->GetNumberOfItems() - 1 >= this->GetIndex() ) {
			this->Mapper->GetClippingPlanes()->ReplaceItem( this->GetIndex(), plane );
		}
		else {
			this->Mapper->AddClippingPlane(plane);
		}
		plane->Delete();
	}
}

void vtkPlaneWidgetCallback::SetMapper(vtkMapper* m) 
{
	this->Mapper = m; 
}

//////////////////////////////////////////////////////////////////////
// implementation of class vtkViewKeyEventCallback 
//////////////////////////////////////////////////////////////////////
vtkViewKeyEventCallback::vtkViewKeyEventCallback()
{
}

vtkViewKeyEventCallback* vtkViewKeyEventCallback::New()
{
	return new vtkViewKeyEventCallback;
}

void vtkViewKeyEventCallback::Execute(vtkObject *caller, unsigned long eid, void* edata)
{
	//CLegiMainWindow *legiWin = reinterpret_cast<CLegiMainWindow*> (caller);

	cout << "eid = " << eid << "\n";
	cout << "edata = " << (*(int*)edata) << "\n";
}

//////////////////////////////////////////////////////////////////////
// implementation of class CLegiMainWindow
//////////////////////////////////////////////////////////////////////
CLegiMainWindow::CLegiMainWindow(int argc, char** argv, vtkMultiProcessController* controller, QWidget* parent, Qt::WindowFlags f)
	: QMainWindow(parent, f), CApplication(argc, argv),
	m_strfnhelp(""),
	m_strfntask(""),
	m_strfnskeleton(""),
	m_nselbox(1),
	m_lod(5),
	m_fRadius(0.25),

	m_pImgRender(NULL),
	m_pstlineModel(NULL),
	m_render( vtkSmartPointer<vtkRenderer>::New() ),
	m_boxWidget(NULL),
	m_oldsz(0, 0),
	m_fnData(""),
	m_fnVolume(""),
	m_fnGeometry(""),
	m_nHaloType(0),
	m_nHaloWidth(1),
	m_bDepthHalo(false),
	m_bDepthSize(false),
	m_bDepthTransparency(false),
	m_bDepthColor(false),
	m_bDepthColorLAB(false),
	m_bDepthValue(false),
	m_bSizeByFA(false),
	m_nCurMethodIdx(0),
	m_nCurPresetIdx(0),
	m_bLighting(true),
	m_bCapping(false),
	m_bHatching(false),
	m_curFocus(FOCUS_GEO),
	m_bInit(false),
	m_bFirstHalo(true),
	m_bCurveMapping(false),
	m_controller ( controller ),
	m_nKey(0),
	m_nFlip(0)
{
	setWindowTitle( "GUItestbed for LegiDTI v1.0" );

	setupUi( this );
	statusbar->setStatusTip ( "recent happenings" );

	__init_volrender_methods();
	__init_tf_presets();

	m_transparency.update(0.0, 1.0);
	m_value.update(0.0, 1.0);

	m_pImgRender = new imgVolRender(this);
	m_pstlineModel = new vtkTgDataReader;

	connect( actionLoadVolume, SIGNAL( triggered() ), this, SLOT (onactionLoadVolume()) );
	connect( actionLoadGeometry, SIGNAL( triggered() ), this, SLOT (onactionLoadGeometry()) );
	connect( actionLoadData, SIGNAL( triggered() ), this, SLOT (onactionLoadData()) );
	connect( actionClose_current, SIGNAL( triggered() ), this, SLOT (onactionClose_current()) );

	connect( actionVolumeRender, SIGNAL( triggered() ), this, SLOT (onactionVolumeRender()) );
	connect( actionGeometryRender, SIGNAL( triggered() ), this, SLOT (onactionGeometryRender()) );
	connect( actionMultipleVolumesRender, SIGNAL( triggered() ), this, SLOT (onactionMultipleVolumesRender()) );
	connect( actionCompositeRender, SIGNAL( triggered() ), this, SLOT (onactionCompositeRender()) );

	connect( actionTF_customize, SIGNAL( triggered() ), this, SLOT (onactionTF_customize()) );
	connect( actionSettings, SIGNAL( triggered() ), this, SLOT (onactionSettings()) );

	//////////////////////////////////////////////////////////////////
	connect( actionLoadScript, SIGNAL( triggered() ), this, SLOT (onactionLoadScript()) );	
	connect( actionSaveScript, SIGNAL( triggered() ), this, SLOT (onactionSaveScript()) );	
	connect( actionRunScript, SIGNAL( triggered() ), this, SLOT (onactionRunScript()) );	
	connect( actionScripting, SIGNAL( triggered() ), this, SLOT (onactionScripting()) );	
	connect( actionClearOutput, SIGNAL( triggered() ), this, SLOT (onactionClearOutput()) );	

	//////////////////////////////////////////////////////////////////
	connect( comboBoxVolRenderMethods, SIGNAL( currentIndexChanged(int) ), this, SLOT ( onVolRenderMethodChanged(int) ) );
	connect( comboBoxVolRenderPresets, SIGNAL( currentIndexChanged(int) ), this, SLOT ( onVolRenderPresetChanged(int) ) );

	connect( checkBoxTubeHalos, SIGNAL( stateChanged(int) ), this, SLOT (onHaloStateChanged(int)) );
	connect( checkBoxDepthSize, SIGNAL( stateChanged(int) ), this, SLOT (onTubeSizeStateChanged(int)) );
	connect( checkBoxDepthTrans, SIGNAL( stateChanged(int) ), this, SLOT (onTubeAlphaStateChanged(int)) );
	connect( checkBoxDepthColor, SIGNAL( stateChanged(int) ), this, SLOT (onTubeColorStateChanged(int)) );

	connect( doubleSpinBoxTubeSize, SIGNAL( valueChanged(double) ), this, SLOT (onTubeSizeChanged(double)) );
	connect( doubleSpinBoxTubeSizeScale, SIGNAL( valueChanged(double) ), this, SLOT (onTubeSizeScaleChanged(double)) );

	connect( doubleSpinBoxAlphaStart, SIGNAL( valueChanged(double) ), this, SLOT (onTubeAlphaStartChanged(double)) );
	connect( doubleSpinBoxAlphaEnd, SIGNAL( valueChanged(double) ), this, SLOT (onTubeAlphaEndChanged(double)) );

	connect( doubleSpinBoxHueStart, SIGNAL( valueChanged(double) ), this, SLOT (onTubeHueStartChanged(double)) );
	connect( doubleSpinBoxHueEnd, SIGNAL( valueChanged(double) ), this, SLOT (onTubeHueEndChanged(double)) );
	connect( doubleSpinBoxSatuStart, SIGNAL( valueChanged(double) ), this, SLOT (onTubeSatuStartChanged(double)) );
	connect( doubleSpinBoxSatuEnd, SIGNAL( valueChanged(double) ), this, SLOT (onTubeSatuEndChanged(double)) );
	connect( doubleSpinBoxValueStart, SIGNAL( valueChanged(double) ), this, SLOT (onTubeValueStartChanged(double)) );
	connect( doubleSpinBoxValueEnd, SIGNAL( valueChanged(double) ), this, SLOT (onTubeValueEndChanged(double)) );

	connect( checkBoxDepthColorLAB, SIGNAL ( stateChanged(int) ), this, SLOT (onTubeColorLABStateChanged(int)) );

	connect( checkBoxDepthValue, SIGNAL (stateChanged(int)), this, SLOT (onTubeDValueStateChanged(int)) );
	connect( doubleSpinBoxDValueStart, SIGNAL( valueChanged(double) ), this, SLOT (onTubeDValueStartChanged(double)) );
	connect( doubleSpinBoxDValueEnd, SIGNAL( valueChanged(double) ), this, SLOT (onTubeDValueEndChanged(double)) );

	connect( sliderHaloWidth, SIGNAL( valueChanged(int) ), this, SLOT (onHaloWidthChanged(int)) );

	connect( checkBoxHatching, SIGNAL( stateChanged(int) ), this, SLOT (onHatchingStateChanged(int)) );

	connect( pushButtonApply, SIGNAL( released() ), this, SLOT (onButtonApply()) );

	connect( pushButtonRun, SIGNAL( released() ), this, SLOT (onButtonRun()) );
	//////////////////////////////////////////////////////////////////
	
	m_render->SetBackground( 0.4392, 0.5020, 0.5647 );

	//vtkSmartPointer< vtkInteractorStyleTrackballCamera > style = vtkSmartPointer< vtkInteractorStyleTrackballCamera >::New();
	vtkLegiInteractorStyle *style = vtkLegiInteractorStyle::New();

	m_dsort = vtkDepthSortPoints::New();
	m_dsort->SetController( m_controller );
	style->agent = m_dsort;

	m_linesort = vtkDepthSortPoints::New();
	m_linesort->SetController( m_controller );
	style->agent2 = m_linesort;

	m_streamtube = vtkTubeFilter::New();
	style->streamtube = m_streamtube;

	m_actor = vtkSmartPointer<vtkActor>::New();
	style->actor = m_actor;

	style->SetController(m_controller);
	renderView->GetInteractor()->SetInteractorStyle( style );
	style->Delete();

	m_fiberSelector = vtkSmartPointer<vtkThresholdedPolyDataFilter>::New();
	m_fiberSelector->ThresholdBetween(1,5);

	m_faSelector = vtkSmartPointer<vtkThresholdedPolyDataFilter>::New();
	m_faSelector->ThresholdBetween(0,1);

	qvtkConnections = vtkEventQtSlotConnect::New();
	qvtkConnections->Connect( renderView->GetRenderWindow()->GetInteractor(),
							vtkCommand::KeyPressEvent, 
							this,
							SLOT ( onKeys(vtkObject*,unsigned long,void*,void*,vtkCommand*) ) );

	m_boxWidget = vtkBoxWidget::New();

	m_boxWidget->SetInteractor(renderView->GetInteractor());
	m_boxWidget->SetPlaceFactor(1.0);
	m_boxWidget->InsideOutOn();

	m_haloactor = vtkSmartPointer<vtkActor>::New();
	m_colorbar = vtkSmartPointer<vtkScalarBarActor>::New();

	m_camera = m_render->MakeCamera();
	
	// HSV will not be used for the first doctor meeting

	/////////////////////////////////////////////////////////////
	addOption('f', true, "input-file-name", "the name of source file"
			" containing geometry and in the format of tgdata");
	addOption('r', true, "tube-radius", "fixed radius of the tubes"
			" to generate");
	addOption('l', true, "lod", "level ot detail controlling the tube"
			" generation, it is expected to impact the smoothness of tubes");
	addOption('p', true, "prompt-text", "a file of interaction help prompt");
	addOption('t', true, "task-list", "a file containing a list of "
			"visualization tasks");
	addOption('s', true, "skeletonic-geometry", "a file containing geometry"
		    " of bundle skeletons also in the format of tgdata");
	addOption('j', true, "boxpos", "files of box position");
	addOption('i', true, "flip model", "boolean 0/1 indicating if to flip the model initially");

	// HSV will not be used for the first doctor meeting
	//checkBoxDepthColor->hide();
	label_hue->hide();
	label_saturation->hide();
	label_value->hide();
	doubleSpinBoxHueStart->hide();
	doubleSpinBoxHueEnd->hide();
	doubleSpinBoxSatuStart->hide();
	doubleSpinBoxSatuEnd->hide();
	doubleSpinBoxValueStart->hide();
	doubleSpinBoxValueEnd->hide();

	checkBoxHatching->hide();

	dockWidget->hide();
}

CLegiMainWindow::~CLegiMainWindow()
{
	delete m_pImgRender;
	delete m_pstlineModel;
	if ( m_boxWidget ) {
		m_boxWidget->Delete();
		m_boxWidget = NULL;
	}

	for (size_t i = 0; i < m_boxes.size(); i++) {
		m_boxes[i]->Delete();
	}

	m_dsort->Delete();
	m_linesort->Delete();
	m_streamtube->Delete();
	qvtkConnections->Delete();
}

int CLegiMainWindow::handleOptions(int optv) 
{
	switch( optv ) {
		case 'f':
			m_fnGeometry= optarg;
			return 0;
		case 'p':
			{
				m_strfnhelp = optarg;
				return 0;
			}
			break;
		case 'r':
			m_fRadius = strtof(optarg, NULL);
			if ( m_fRadius < 0.1 ) {
				cerr << "value for radius is illicit, should be >= .1\n";
				return -1;
			}
			return 0;
		case 'l':
			{
				int lod = atoi(optarg);
				if ( lod >= 2 ) {
					m_lod = lod;
					return 0;
				}
				else {
					cerr << "value for lod is illict, should be >= 2.\n";
					return -1;
				}
			}
			break;
		case 't':
			{
				m_strfntask = optarg;
				return 0;
			}
			break;
		case 's':
			{
				m_strfnskeleton = optarg;
				return 0;
			}
			break;
		case 'j':
			{
				m_strfnBoxes.push_back(optarg);
				return 0;
			}
			break;
		case 'i':
			{
				m_nFlip = atoi(optarg);
				return 0;
			}
			break;
		default:
			return CApplication::handleOptions( optv );
	}
	return 1;
}

int CLegiMainWindow::mainstay()
{
	int numProcs = m_controller->GetNumberOfProcesses();
	int myId = m_controller->GetLocalProcessId();

	m_controller->Barrier();
	if ( myId == 0 ) {
		if ( !m_pstlineModel->Load( m_fnGeometry.c_str(), true, true) ) {
			QMessageBox::critical(this, "Error...", "failed to load geometry data provided.");
			m_fnGeometry = "";
		}

		if ( ! m_controller->Broadcast ( m_pstlineModel->GetOutput(), 0 ) ) {
			cout << "sending polydata failed.\n";
		}

		if ( ! m_controller->Broadcast ( m_pstlineModel->GetPolyColor(), 0 ) ) {
			cout << "sending polycolor failed.\n";
		}
	}
	else {
		//m_pstlineModel->GetOutput() = vtkPolyData::SafeDownCast(m_controller->ReceiveDataObject(0, 10));
		if ( ! m_controller->Broadcast ( m_pstlineModel->GetOutput(), 0) ) {
			cout << "receiving polydata failed.\n";
		}

		if ( ! m_controller->Broadcast ( m_pstlineModel->GetPolyColor(), 0) ) {
			cout << "receiving polycolor failed.\n";
		}
	}

	/*
	cout << "in proc " << myId << "\n";
	m_pstlineModel->GetOutput()->PrintSelf(cout, vtkIndent());
	m_pstlineModel->GetPolyColor()->PrintSelf(cout, vtkIndent());
	*/
	m_pstlineModel->GetOutput()->GetBounds(m_wholeExtent);

	m_pstlineModel->SetParallelParams(numProcs, myId, true, true);

	__load_colormap();

	setWindowTitle( "SvlDTI demo 1.0" );

	//m_render->SetOcclusionRatio( .8 );

	/*
	cout << "current occlusion ratio: " << m_render->GetOcclusionRatio() << "\n";

	cout << " =-==================== in proc " << myId << " ===================\n";
	m_pstlineModel->GetOutput()->GetPointData()->GetArray("FA COLORS")->Modified();
	m_pstlineModel->GetOutput()->GetPointData()->GetArray("FA COLORS")->PrintSelf(cout, vtkIndent());
	*/

	show();

	/*
	cout << "show done.\n";
	*/

	__add_axes();

	__add_planes();

	if ( myId == 0 ) {

		if ( m_strfntask != "" ) {
			m_taskbox->loadFromFile( m_strfntask.c_str() );
			//m_taskbox->setRenderWindow( renderView->GetRenderWindow() );
			m_taskbox->setUseType( UT_TASKBOX );

			m_render->AddActor2D( m_taskbox );
		}

		if ( m_strfnhelp != "" ) {
			m_helptext->loadFromFile( m_strfnhelp.c_str() );
			//m_helptext->setRenderWindow( renderView->GetRenderWindow() );
			m_helptext->setUseType( UT_HELPTEXT );

			m_render->AddActor2D( m_helptext );
		}

		if ( m_cout.isswitchon() ) {
			m_cout.switchtime(true);

			// start event recording
			m_recorder = vtkSmartPointer<vtkLegiInteractionRecorder>::New();
			m_recorder->SetInteractor( renderView->GetRenderWindow()->GetInteractor() );
			m_recorder->SetOutputStream( &m_cout );
			m_recorder->StudyLogStyleOn();
			m_recorder->EnabledOn();
			m_recorder->Record();
		}

		m_cout << "started.\n";
	}

	/*
	cout << "proc id: " << myId << " #layers: " << renderView->GetRenderWindow()->GetNumberOfLayers() << "\n";

	m_render->ResetCamera();
	m_camera->Zoom (2.0);
	m_render->ResetCameraClippingRange();
	renderView->update();
	*/

	/*
	if ( m_nFlip ) {
		vtkSmartPointer<vtkTransform> ctf = vtkSmartPointer<vtkTransform>::New();
		ctf->RotateY(180);
		m_camera->ApplyTransform( ctf );
	}
	*/

	m_cout << "Loading finished \n";
	return CApplication::mainstay();
}

void CLegiMainWindow::keyPressEvent (QKeyEvent* event)
{
	QMainWindow::keyPressEvent( event );
}

void CLegiMainWindow::resizeEvent( QResizeEvent* event )
{
	QMainWindow::resizeEvent( event );
	if (m_oldsz.isNull())
	{
		m_oldsz = event->size();
	}
	else
	{
		QSize sz = event->size();
		QSize osz = this->m_oldsz;

		float wf = sz.width()*1.0/osz.width();
		float hf = sz.height()*1.0/osz.height();

		QSize orsz = this->renderView->size();
		orsz.setWidth ( orsz.width() * wf );
		orsz.setHeight( orsz.height() * hf );
		//this->renderView->resize( orsz );
		this->renderView->resize( sz );

		this->m_oldsz = sz;
	}
}

void CLegiMainWindow::draw()
{
	onactionGeometryRender();
}

void CLegiMainWindow::show()
{
	QMainWindow::show();
	draw();
}

QVTKWidget*& CLegiMainWindow::getRenderView()
{
	return renderView;
}

void CLegiMainWindow::addBoxWidget()
{	
	/*
	if ( m_boxWidget ) {
		m_boxWidget->Delete();
		m_boxWidget = NULL;
	}
	*/
	m_boxWidget->Off();
	m_boxWidget->RemoveAllObservers();

	vtkVolumeMapper* mapper = vtkVolumeMapper::SafeDownCast(m_pImgRender->getVol()->GetMapper());
	vtkBoxWidgetCallback *callback = vtkBoxWidgetCallback::New();
	callback->SetMapper(mapper);
	 
	m_boxWidget->SetInput(mapper->GetInput());
	m_boxWidget->AddObserver(vtkCommand::InteractionEvent, callback);
	callback->Delete();
	m_boxWidget->SetProp3D(m_pImgRender->getVol());
	m_boxWidget->PlaceWidget();
	m_boxWidget->On();
	//m_boxWidget->GetSelectedFaceProperty()->SetOpacity(0.0);
}


void CLegiMainWindow::onactionLoadVolume()
{
	QString dir = QFileDialog::getExistingDirectory(this, tr("select volume to load"),
			"/home/chap", QFileDialog::ShowDirsOnly);
	if (dir.isEmpty()) return;

	m_fnVolume = dir.toStdString();
	if ( !m_pImgRender->mount(m_fnVolume.c_str(), true) ) {
		QMessageBox::critical(this, "Error...", "failed to load volume images provided.");
		m_fnVolume = "";
	}
}

void CLegiMainWindow::onactionLoadGeometry()
{
	QString fn = QFileDialog::getOpenFileName(this, tr("select geometry to load"), 
			"/home/chap", tr("Data (*.data);; All (*.*)"));
	if (fn.isEmpty()) return;

	m_fnGeometry = fn.toStdString();
	if ( !m_pstlineModel->Load( m_fnGeometry.c_str() ) ) {
		QMessageBox::critical(this, "Error...", "failed to load geometry data provided.");
		m_fnGeometry = "";
	}
}

void CLegiMainWindow::onactionLoadData()
{
	QString fn = QFileDialog::getOpenFileName(this, tr("select dataset to load"), 
			"/home/chap", tr("NIfTI (*.nii *.nii.gz);; All (*.*)"));
	if (fn.isEmpty()) return;

	m_fnData = fn.toStdString();
	if ( !m_pImgRender->mount(m_fnData.c_str()) ) {
		QMessageBox::critical(this, "Error...", "failed to load composite image data provided.");
		m_fnData = "";
	}
}

void CLegiMainWindow::onactionClose_current()
{
	__removeAllVolumes();
	__removeAllActors();
	cout << "Numer of Actor in the render after removing all: " << (m_render->VisibleActorCount()) << "\n";
	cout << "Numer of Volumes in the render after removing all: " << (m_render->VisibleVolumeCount()) << "\n";
	renderView->update();
}

void CLegiMainWindow::onactionVolumeRender()
{
	if ( m_fnVolume.length() < 1 ) {
		return;
	}

	__removeAllVolumes();

	m_render->AddVolume(m_pImgRender->getVol());
	cout << "Numer of Volumes in the render: " << (m_render->VisibleVolumeCount()) << "\n";

	renderView->GetRenderWindow()->SetAlphaBitPlanes(1);
	renderView->GetRenderWindow()->SetMultiSamples(0);
	m_render->SetUseDepthPeeling(1);
	m_render->SetMaximumNumberOfPeels(100);
	m_render->SetOcclusionRatio(0.1);

	if ( ! renderView->GetRenderWindow()->HasRenderer( m_render ) )
		renderView->GetRenderWindow()->AddRenderer(m_render);

	m_curFocus = FOCUS_VOL;
	renderView->update();
	addBoxWidget();
}

void CLegiMainWindow::onactionGeometryRender()
{
	if ( m_fnGeometry.length() < 1 ) {
		return;
	}

	/*
	if ( m_curFocus != FOCUS_GEO ) 
	{
		__removeAllVolumes();
		if ( m_boxWidget ) {
			m_boxWidget->SetEnabled( false );
		}
		m_bInit = false;
		m_nHaloType = 0;
		__renderTubes(m_nHaloType);
	}
	*/

	__uniformTubeRendering();

	/*
	if ( m_bDepthHalo || m_bDepthColorLAB || m_bDepthValue || m_bDepthTransparency || m_bDepthSize ) {
		__uniformTubeRendering();
	}
	else if (m_bHatching) {
		__add_texture_strokes();
	}
	else {
		__renderTubes(m_nHaloType);
	}

	cout << "Numer of Actor in the render: " << (m_render->VisibleActorCount()) << "\n";

	renderView->GetRenderWindow()->SetAlphaBitPlanes(1);
	renderView->GetRenderWindow()->SetMultiSamples(0);
	m_render->SetUseDepthPeeling(1);
	m_render->SetMaximumNumberOfPeels(100);
	m_render->SetOcclusionRatio(0.5);
	cout << "depth peeling flag: " << m_render->GetLastRenderingUsedDepthPeeling() << "\n";
	*/

	if ( ! renderView->GetRenderWindow()->HasRenderer( m_render ) ) {
		renderView->GetRenderWindow()->AddRenderer(m_render);
	}

	m_curFocus = FOCUS_GEO;
	renderView->update();
}

void CLegiMainWindow::onactionMultipleVolumesRender()
{
	if ( m_fnData.length() < 1 ) {
		return;
	}

	__removeAllVolumes();

	m_render->AddVolume(m_pImgRender->getVol());
	cout << "Numer of Volumes in the render: " << (m_render->VisibleVolumeCount()) << "\n";

	renderView->GetRenderWindow()->SetAlphaBitPlanes(1);
	renderView->GetRenderWindow()->SetMultiSamples(0);
	m_render->SetUseDepthPeeling(1);
	m_render->SetMaximumNumberOfPeels(100);
	m_render->SetOcclusionRatio(0.1);

	if ( ! renderView->GetRenderWindow()->HasRenderer( m_render ) )
		renderView->GetRenderWindow()->AddRenderer(m_render);

	m_curFocus = FOCUS_MVOL;
	renderView->update();
	addBoxWidget();
}

void CLegiMainWindow::onactionCompositeRender()
{
	if ( m_fnData.length() < 1 || m_fnGeometry.length() < 1) {
		return;
	}

	__removeAllVolumes();

	m_render->AddVolume(m_pImgRender->getVol());
	cout << "Numer of Volumes in the render: " << (m_render->VisibleVolumeCount()) << "\n";

	renderView->GetRenderWindow()->SetAlphaBitPlanes(1);
	renderView->GetRenderWindow()->SetMultiSamples(0);
	m_render->SetUseDepthPeeling(1);
	m_render->SetMaximumNumberOfPeels(100);
	m_render->SetOcclusionRatio(0.1);

	if ( ! renderView->GetRenderWindow()->HasRenderer( m_render ) )
		renderView->GetRenderWindow()->AddRenderer(m_render);

	m_bInit = false;
	m_nHaloType = 0;
	if ( m_boxWidget ) {
		m_boxWidget->SetEnabled( false );
	}
	__renderTubes(m_nHaloType);
	m_curFocus = FOCUS_COMPOSITE;
	renderView->update();
	addBoxWidget();
}

void CLegiMainWindow::onactionTF_customize()
{
	if (m_fnVolume.length() < 1 && m_fnData.length() < 1 ) { // the TF widget has never been necessary
		return;
	}

	if ( m_pImgRender->t_graph->isVisible() ) {
		m_pImgRender->t_graph->setVisible( false );
	}
	else {
		m_pImgRender->t_graph->setVisible( true );
	}
}

void CLegiMainWindow::onactionSettings()
{
	if ( dockWidget->isVisible() ) {
		dockWidget->setVisible( false );
	}
	else {
		dockWidget->setVisible( true );
	}
}

void CLegiMainWindow::onactionLoadScript()
{
	QString fn = QFileDialog::getOpenFileName(this, tr("select script to load"), 
			"/home/chap", tr("SvlDTI (*.svls *.txt);; All (*.*)"));
	if (fn.isEmpty()) return;

	QFile fh(fn);
	if ( !fh.open(QIODevice::ReadOnly | QIODevice::Text) ) {
		cerr << "Failed to open file: " << fn.toStdString() << endl;
		return;
	}

	char buf[1024];
	while ( ! fh.atEnd() ) {
		if ( fh.readLine( buf, sizeof buf ) != -1 ) {
			textEditScript->insertPlainText( buf );
		}
	}

	fh.close();
	statusbar->showMessage( tr("script loaded successfully.") );
}

void CLegiMainWindow::onactionSaveScript()
{
	QString script = textEditScript->toPlainText();
	if ( script.isEmpty() ) {
		return;
	}

	QString fn = QFileDialog::getSaveFileName(this, tr("indicate file to save the script"),
			"/home/chap/untitled.svls", tr("SvlDTI (*.svls);; Text (*.txt);; All (*.*)"));
	if (fn.isEmpty()) return;

	QFile fh(fn);
	if ( !fh.open(QIODevice::WriteOnly | QIODevice::Text) ) {
		cerr << "Failed to open file for writing: " << fn.toStdString() << endl;
		return;
	}

	fh.write( script.toStdString().c_str(), qstrlen(script.toStdString().c_str()) );

	fh.close();
	statusbar->showMessage( tr("script saved successfully.") );
}

void CLegiMainWindow::onactionRunScript()
{
	onButtonRun();
}

void CLegiMainWindow::onactionScripting()
{
	if ( dockWidgetScript->isVisible() ) {
		dockWidgetScript->setVisible( false );
	}
	else {
		dockWidgetScript->setVisible( true );
	}
}

void CLegiMainWindow::onactionClearOutput()
{
	textEditDebug->setText( "" );	
}

void CLegiMainWindow::onKeys(vtkObject* obj,unsigned long eid,void* client_data,void* data2,vtkCommand* command)
{
	vtkRenderWindowInteractor *iren = vtkRenderWindowInteractor::SafeDownCast( obj );
	command->AbortFlagOn();

	switch (iren->GetKeyCode()){
		case 27:
			if ( this->m_controller->GetLocalProcessId() == 0 ) {
				//this->~CLegiMainWindow();
				this->m_controller->TriggerRMIOnAllChildren(vtkMultiProcessController::BREAK_RMI_TAG);
				m_cout << "quit.";
				m_cout.switchtime(false);
				((QWidget*)this)->close();
				((QWidget*)centralwidget->parent())->close();
				this->m_controller->Finalize();
				vtkMultiProcessController::SetGlobalController(NULL);
				m_cout << "\n";
				qApp->quit();
				exit(1);
			}
			break;
		case 'i':
			m_bLighting = ! m_bLighting;
			break;
		case 'h':
			m_nHaloType = 1;
			break;
		case 'j':
			m_nHaloType = 2;
			break;
		case 'k':
			m_nHaloType = 0;
			break;
		case 'l':
			m_nHaloType = 3;
			break;
		case 'm':
			m_nHaloType = 4;
			break;
		case 'd':
			m_nHaloType = 5;
			break;
		case 's':
			m_nHaloType = 6;
			break;
		case 't':
			m_nHaloType = 7;
			break;
		case 'r':
			m_nHaloType = 8;
			break;
		case 'o':
			m_nHaloType = 9;
			break;
		case 'b':
			{
				if ( m_render->GetVolumes()->GetNumberOfItems() < 1 ) {
					cout << "no image volumes added.\n";
					return;
				}

				if ( m_boxWidget ) {
					m_boxWidget->SetEnabled( ! m_boxWidget->GetEnabled() );
				}
			}
			return;
		case 'c':
			m_bCapping = !m_bCapping;
			break;
		case 'v':
			m_bCurveMapping = !m_bCurveMapping;
			break;
		case 'y':
			{
				selfRotate(0);
			}
			break;
		default:
			return;
	}
	onactionGeometryRender();
}

void CLegiMainWindow::onHaloStateChanged(int state)
{
	m_bDepthHalo = ( Qt::Checked == state );
	/*
	if ( m_bDepthHalo ) {
		m_nHaloType = 1;
	}
	else {
		m_nHaloType = 0;
	}
	onactionGeometryRender();
	if ( m_bDepthHalo ) {
		__uniform_halos();
		m_render->AddActor( m_haloactor );
	}
	else {
		m_render->RemoveActor( m_haloactor );
		renderView->update();
	}
	*/
}

void CLegiMainWindow::onVolRenderMethodChanged(int index)
{
	if (m_fnVolume.length() < 1 && m_fnData.length() < 1 ) { // the TF widget has never been necessary
		return;
	}
	m_nCurMethodIdx = index;

	if ( m_curFocus == FOCUS_VOL && m_fnVolume.length() >=1 && m_pImgRender->mount(m_fnVolume.c_str(), true) ) {
		onactionVolumeRender();
		return;
	}
	if ( m_curFocus == FOCUS_MVOL && m_fnData.length() >=1 && m_pImgRender->mount(m_fnData.c_str()) ) {
		onactionMultipleVolumesRender();
		return;
	}
	if ( m_curFocus == FOCUS_COMPOSITE && m_fnData.length() >=1 && m_pImgRender->mount(m_fnData.c_str()) ) {
		onactionCompositeRender();
	}
}

void CLegiMainWindow::onVolRenderPresetChanged(int index)
{
	if (m_fnVolume.length() < 1 && m_fnData.length() < 1 ) { // the TF widget has never been necessary
		return;
	}

	m_nCurPresetIdx = index;

	if ( m_nCurPresetIdx != VP_AUTO ) {
		if ( m_pImgRender->t_graph->isVisible() ) {
			m_pImgRender->t_graph->setVisible( false );
		}
	}
	else {
		if ( !m_pImgRender->t_graph->isVisible() ) {
			m_pImgRender->t_graph->setVisible( true );
		}
	}

	if ( m_curFocus == FOCUS_VOL && m_fnVolume.length() >=1 && m_pImgRender->mount(m_fnVolume.c_str(), true) ) {
		onactionVolumeRender();
		return;
	}
	if ( m_curFocus == FOCUS_MVOL && m_fnData.length() >=1 && m_pImgRender->mount(m_fnData.c_str()) ) {
		onactionMultipleVolumesRender();
		return;
	}
	if ( m_curFocus == FOCUS_COMPOSITE && m_fnData.length() >=1 && m_pImgRender->mount(m_fnData.c_str()) ) {
		onactionCompositeRender();
	}
}

void CLegiMainWindow::onTubeSizeStateChanged(int state)
{
	m_bDepthSize = ( Qt::Checked == state );
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeSizeChanged(double d)
{
	m_dptsize.size = d;
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeSizeScaleChanged(double d)
{
	m_dptsize.scale = d;
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeAlphaStateChanged(int state)
{
	m_bDepthTransparency = ( Qt::Checked == state );
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeAlphaStartChanged(double d)
{
	m_transparency[0] = d;
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeAlphaEndChanged(double d)
{
	m_transparency[1] = d;
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeColorStateChanged(int state)
{
	m_bDepthColor = ( Qt::Checked == state );
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeHueStartChanged(double d)
{
	m_dptcolor.hue[0] = d;
	onactionGeometryRender();
}

void CLegiMainWindow::onTubeHueEndChanged(double d)
{
	m_dptcolor.hue[1] = d;
	onactionGeometryRender();
}

void CLegiMainWindow::onTubeSatuStartChanged(double d)
{
	m_dptcolor.satu[0] = d;
	onactionGeometryRender();
}

void CLegiMainWindow::onTubeSatuEndChanged(double d)
{
	m_dptcolor.satu[1] = d;
	onactionGeometryRender();
}

void CLegiMainWindow::onTubeValueStartChanged(double d)
{
	m_dptcolor.value[0] = d;
	onactionGeometryRender();
}

void CLegiMainWindow::onTubeValueEndChanged(double d)
{
	m_dptcolor.value[1] = d;
	onactionGeometryRender();
}

void CLegiMainWindow::onTubeColorLABStateChanged(int state)
{
	m_bDepthColorLAB = ( Qt::Checked == state );
	/*
	onactionGeometryRender();
	if ( !m_bDepthColorLAB ) {
		m_render->RemoveActor2D ( m_colorbar );
		renderView->update();
	}
	*/
}

void CLegiMainWindow::onTubeDValueStateChanged(int state)
{
	m_bDepthValue = ( Qt::Checked == state );
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeDValueStartChanged(double d)
{
	m_value[0] = d;
	//onactionGeometryRender();
}

void CLegiMainWindow::onTubeDValueEndChanged(double d)
{
	m_value[1] = d;
	//onactionGeometryRender();
}

void CLegiMainWindow::onHaloWidthChanged(int i)
{
	if ( !m_bDepthHalo ) return;
	m_nHaloWidth = i;
	/*
	if ( m_bDepthHalo ) {
		__uniform_halos();
	}
	onactionGeometryRender();
	*/
}

void CLegiMainWindow::onHatchingStateChanged(int state)
{
	m_bHatching = ( Qt::Checked == state );
	onactionGeometryRender();
}

void CLegiMainWindow::onButtonApply()
{
	if ( m_bDepthHalo ) {
		__uniform_halos();
		m_render->AddActor( m_haloactor );
	}
	else {
		m_render->RemoveActor( m_haloactor );
		renderView->update();
	}

	if ( m_bDepthHalo ) {
		__uniform_halos();
	}

	if ( !m_bDepthColorLAB || !m_bDepthColor ) {
		m_render->RemoveActor2D ( m_colorbar );
		renderView->update();
	}

	onactionGeometryRender();
}

void CLegiMainWindow::onButtonRun()
{
	QString script = textEditScript->toPlainText();

	if ( script.isEmpty() ) return;

	/* resume everything */
	m_fiberSelector->ThresholdBetween(1,5);
	m_faSelector->ThresholdBetween(0,1);

	/*
	QMessageBox::information(this, "Test...", script);
	m_fiberSelector->ThresholdBetween(0.5, 0.8);
	*/

	//
	/*
	 * Here the simple interpreter of SVL script starts
	 * In this early stage, this does not work like a real interpreter or compiler,which will conduct multiple scans of
	 * the source, but rather interpret line by line. This essentially assumes that only sequential logic flow is
	 * supported.
	 */
	QRegExp linesep("\\n+");
	int n = 0;

	QString curline = script.section(linesep, n, n, QString::SectionSkipEmpty);
	while ( ! curline.isEmpty() ) {
		// interpret the current line of script
		/* QMessageBox::information(this, "Execute...", curline); */
		if (0 != __execLine( curline.trimmed() )) {
			//QMessageBox::critical(this, "Execution stopped at following statement...", curline);
			__debugOutput("Execution stopped at following statement : \n\t" + curline, DL_LOG);
			//break;
			return;
		}

		// move to the next one 
		n++;
		curline = script.section(linesep, n, n, QString::SectionSkipEmpty);
	}

	onactionGeometryRender();
	renderView->update();
}

void CLegiMainWindow::__renderDepthSortTubes()
{
	vtkSmartPointer<vtkTubeFilter> streamTube = vtkSmartPointer<vtkTubeFilter>::New();
	streamTube->SetInput( m_pstlineModel->GetOutput() );
	streamTube->SetRadius(0.25);
	streamTube->SetNumberOfSides(12);

	vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();

	vtkSmartPointer<vtkDepthSortPolyData> dsort = vtkSmartPointer<vtkDepthSortPolyData>::New();
	dsort->SetInputConnection( streamTube->GetOutputPort() );
	dsort->SetDirectionToBackToFront();
	//dsort->SetDirectionToFrontToBack();
	//dsort->SetDepthSortModeToParametricCenter();
	//dsort->SetDirectionToSpecifiedVector();
	dsort->SetVector(0,0,1);
	dsort->SetCamera( camera );
	dsort->SortScalarsOn();
	dsort->Update();

	//vtkSmartPointer<vtkTubeHaloMapper> mapStreamTube = vtkSmartPointer<vtkTubeHaloMapper>::New();
	vtkSmartPointer<vtkPolyDataMapper> mapStreamTube = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapStreamTube->SetInputConnection(dsort->GetOutputPort());

	//mapStreamTube->SetScalarRange(0, dsort->GetOutput()->GetNumberOfCells());
	mapStreamTube->SetScalarRange(0, dsort->GetOutput()->GetNumberOfStrips());
	/*
	mapStreamTube->SetColorModeToMapScalars();
	mapStreamTube->ScalarVisibilityOn();
	mapStreamTube->MapScalars(0.5);
	mapStreamTube->UseLookupTableScalarRangeOn();
	*/

	vtkSmartPointer<vtkActor> streamTubeActor = vtkSmartPointer<vtkActor>::New();
	streamTubeActor->SetMapper(mapStreamTube);

	vtkSmartPointer<vtkProperty> tubeProperty = vtkSmartPointer<vtkProperty>::New();
	tubeProperty->SetOpacity(.6);
	//tubeProperty->SetOpacity(1.0);
	tubeProperty->SetColor(1,0,0);
	streamTubeActor->SetProperty( tubeProperty );
	streamTubeActor->RotateX( -72 );

	dsort->SetProp3D( streamTubeActor );
  
	m_render->SetActiveCamera( camera );
	m_render->AddActor( streamTubeActor );
	m_render->ResetCamera();
}

void CLegiMainWindow::__depth_dependent_size()
{
	//vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();

	//vtkSmartPointer<vtkDepthSortPolyData> linesort = vtkSmartPointer<vtkDepthSortPolyData>::New();
	vtkSmartPointer<vtkDepthSortPoints> linesort = vtkSmartPointer<vtkDepthSortPoints>::New();
	linesort->SetInput( m_pstlineModel->GetOutput() );
	linesort->SetDirectionToBackToFront();
	linesort->SetVector(0,0,1);
	linesort->SetCamera( m_camera );
	linesort->SortScalarsOn();
	linesort->Update();

	//vtkDataArray * SortScalars = linesort->GetOutput()->GetPointData()->GetAttribute( vtkDataSetAttributes::SCALARS );
	//SortScalars->PrintSelf( cout, vtkIndent());

	vtkSmartPointer<vtkTubeFilterEx> streamTube = vtkSmartPointer<vtkTubeFilterEx>::New();
	streamTube->SetInputConnection( linesort->GetOutputPort() );
	streamTube->SetRadius(0.03);
	streamTube->SetNumberOfSides(12);
	streamTube->SetRadiusFactor( 20 );
	//streamTube->SetVaryRadiusToVaryRadiusByAbsoluteScalar();
	streamTube->SetVaryRadiusToVaryRadiusByScalar();
	//streamTube->UseDefaultNormalOn();
	streamTube->SetCapping( m_bCapping );
	streamTube->Update();

	m_actor->GetMapper()->SetInputConnection(streamTube->GetOutputPort());
	m_actor->GetMapper()->ScalarVisibilityOff();
	m_actor->GetProperty()->SetColor(1,1,1);
	linesort->SetProp3D( m_actor );
	return;

	vtkSmartPointer<vtkPolyDataMapper> mapStreamTube = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapStreamTube->SetInputConnection(streamTube->GetOutputPort());
	//mapStreamTube->SetScalarRange(0, streamTube->GetOutput()->GetNumberOfCells());
	mapStreamTube->ScalarVisibilityOff();
	//mapStreamTube->SetScalarModeToUsePointData();

	vtkSmartPointer<vtkActor> streamTubeActor = vtkSmartPointer<vtkActor>::New();
	streamTubeActor->SetMapper(mapStreamTube);

	vtkSmartPointer<vtkProperty> tubeProperty = vtkSmartPointer<vtkProperty>::New();
	tubeProperty->SetColor(1,1,1);
	streamTubeActor->SetProperty( tubeProperty );

	linesort->SetProp3D( streamTubeActor );
  
	m_render->SetActiveCamera( m_camera );
	m_render->AddActor( streamTubeActor );
	m_render->ResetCamera();
}

void CLegiMainWindow::__renderLines(int type)
{
	vtkSmartPointer<vtkTubeHaloMapper> mapStreamLines = vtkSmartPointer<vtkTubeHaloMapper>::New();
	mapStreamLines->SetInput( m_pstlineModel->GetOutput() );

	vtkSmartPointer<vtkActor> streamLineActor = vtkSmartPointer<vtkActor>::New();
	streamLineActor ->SetMapper(mapStreamLines);
	streamLineActor->GetProperty()->BackfaceCullingOn();

	m_render->AddViewProp( streamLineActor );
}

void CLegiMainWindow::__renderRibbons()
{
	vtkSmartPointer<vtkRibbonFilter> ribbon = vtkSmartPointer<vtkRibbonFilter>::New();
	ribbon->SetInput ( m_pstlineModel->GetOutput() );

	vtkSmartPointer<vtkPolyDataMapper> mapRibbons = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapRibbons->SetInputConnection( ribbon->GetOutputPort() );

	vtkSmartPointer<vtkActor> ribbonActor = vtkSmartPointer<vtkActor>::New();
	ribbonActor->SetMapper(mapRibbons);
	ribbonActor->GetProperty()->BackfaceCullingOn();

	m_render->AddViewProp( ribbonActor );
}

void CLegiMainWindow::__depth_dependent_transparency()
{
	vtkSmartPointer<vtkTubeFilter> streamTube = vtkSmartPointer<vtkTubeFilter>::New();
	streamTube->SetInput( m_pstlineModel->GetOutput() );
	streamTube->SetRadius(0.25);
	streamTube->SetNumberOfSides(5);

	vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();

	//vtkSmartPointer<vtkDepthSortPolyData> dsort = vtkSmartPointer<vtkDepthSortPolyData>::New();
	vtkSmartPointer<vtkDepthSortPoints> dsort = vtkSmartPointer<vtkDepthSortPoints>::New();
	dsort->SetInputConnection( streamTube->GetOutputPort() );
	//dsort->SetInput( m_pstlineModel->GetOutput() );
	dsort->SetDirectionToBackToFront();
	//dsort->SetDirectionToFrontToBack();
	//dsort->SetDepthSortModeToParametricCenter();
	//dsort->SetDirectionToSpecifiedVector();
	dsort->SetVector(0,0,1);
	//dsort->SetOrigin(0,0,0);
	//dsort->SetCamera( camera );
	dsort->SetCamera( m_camera );
	dsort->SortScalarsOn();
	dsort->Update();

	vtkSmartPointer<vtkPolyDataMapper> mapStreamTube = vtkSmartPointer<vtkPolyDataMapper>::New();
	/*
	mapStreamTube->SetInputConnection(dsort->GetOutputPort());
	*/
	m_actor->GetMapper()->SetInputConnection(dsort->GetOutputPort());

	vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
	lut->SetAlphaRange( 0.01, 1.0 );
	lut->SetHueRange( 0.0, 0.0 );
	lut->SetSaturationRange(0.0, 0.0);
	lut->SetValueRange( 0.0, 1.0);
	//lut->SetAlpha( 0.5 );

	//mapStreamTube->SetLookupTable( lut );
	m_actor->GetMapper()->SetLookupTable( lut );
	//mapStreamTube->SetScalarRange( dsort->GetOutput()->GetCellData()->GetScalars()->GetRange());
	//mapStreamTube->SetScalarRange(0, dsort->GetOutput()->GetNumberOfCells());
	//mapStreamTube->SetScalarRange(0, dsort->GetOutput()->GetNumberOfPoints());
	m_actor->GetMapper()->SetScalarRange(0, dsort->GetOutput()->GetNumberOfPoints());

	return;
	//streamTube->GetOutput()->GetCellData()->GetScalars()->PrintSelf(cout, vtkIndent());
	//mapStreamTube->UseLookupTableScalarRangeOn();
	//mapStreamTube->ScalarVisibilityOn();

	vtkSmartPointer<vtkActor> streamTubeActor = vtkSmartPointer<vtkActor>::New();
	streamTubeActor->SetMapper(mapStreamTube);

	vtkSmartPointer<vtkProperty> tubeProperty = vtkSmartPointer<vtkProperty>::New();
	//tubeProperty->SetRepresentationToWireframe();
	//tubeProperty->SetColor(0,0,0);
	//tubeProperty->SetLineWidth(2.0);

	streamTubeActor->SetProperty( tubeProperty );
  
	dsort->SetProp3D( streamTubeActor );

	streamTubeActor->RotateX( -72 );
	m_render->SetActiveCamera( camera );
	m_render->AddActor( streamTubeActor );
	m_render->ResetCamera();
}

void CLegiMainWindow::__depth_dependent_halos()
{
	vtkSmartPointer<vtkTubeFilter> streamTube = vtkSmartPointer<vtkTubeFilter>::New();
	streamTube->SetInput( m_pstlineModel->GetOutput() );
	streamTube->SetRadius(0.25);
	streamTube->SetNumberOfSides(12);

	vtkSmartPointer<vtkTubeHaloMapper> mapStreamTube = vtkSmartPointer<vtkTubeHaloMapper>::New();
	mapStreamTube->SetInputConnection(streamTube->GetOutputPort());

	vtkSmartPointer<vtkActor> streamTubeActor = vtkSmartPointer<vtkActor>::New();
	streamTubeActor->SetMapper(mapStreamTube);

	vtkSmartPointer<vtkProperty> tubeProperty = vtkSmartPointer<vtkProperty>::New();
	//tubeProperty->SetRepresentationToWireframe();
	//tubeProperty->SetColor(0,0,0);
	//tubeProperty->SetLineWidth(2.0);

	streamTubeActor->SetProperty( tubeProperty );
  
	m_render->AddActor( streamTubeActor );

	__addHaloType1();
}

void CLegiMainWindow::__renderTubes(int type)
{
	if ( type == 2 ) {
		__renderDepthSortTubes();
		return;
	}

	if ( type == 3 ) {
		__renderLines();
		return;
	}

	if ( type == 4 ) {
		__depth_dependent_transparency();
		return;
	}

	if ( type == 5 ) {
		__depth_dependent_halos();
		return;
	}

	if ( type == 6 ) {
		__depth_dependent_size();
		return;
	}

	if ( type == 7 ) {
		__add_texture_strokes();
		return;
	}

	if ( type == 8 ) {
		__renderRibbons();
		return;
	}

	if ( type == 9 ) {
		__iso_surface(true);
		return;
	}

	vtkSmartPointer<vtkTubeFilter> streamTube = vtkSmartPointer<vtkTubeFilter>::New();
	streamTube->SetInput( m_pstlineModel->GetOutput() );
	streamTube->SetRadius(0.25);
	streamTube->SetNumberOfSides(12);
	streamTube->SidesShareVerticesOn();
	streamTube->SetCapping( m_bCapping );

	vtkSmartPointer<vtkPolyDataMapper> mapStreamTube = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapStreamTube->SetInputConnection(streamTube->GetOutputPort());

	/*
	vtkSmartPointer<vtkActor> streamTubeActor = vtkSmartPointer<vtkActor>::New();
	streamTubeActor->SetMapper(mapStreamTube);
	*/
	m_actor->SetMapper( mapStreamTube );

	vtkSmartPointer<vtkProperty> tubeProperty = vtkSmartPointer<vtkProperty>::New();
	//tubeProperty->SetRepresentationToWireframe();
	//tubeProperty->SetColor(0.5,0.5,0);
	//tubeProperty->SetLineWidth(2.0);

	//streamTubeActor->SetProperty( tubeProperty );
	m_actor->SetProperty( tubeProperty );

	if ( !m_bInit ) {
		m_render->SetActiveCamera( m_camera );
		//m_render->AddActor( streamTubeActor );
		m_render->AddActor( m_actor );
		/*
		m_render->ResetCamera();
		m_camera->Zoom (2.0);
		*/
		m_bInit = true;
	}

	switch ( m_nHaloType ) {
		case 0:
			break;
		case 1:
			__addHaloType1();
			break;
		case 2:
		default:
			break;
	}
}

void CLegiMainWindow::__addHaloType1()
{
	vtkSmartPointer<vtkTubeFilter> tubeHalos = vtkSmartPointer<vtkTubeFilter>::New();
	tubeHalos->SetInput( m_pstlineModel->GetOutput() );
	tubeHalos->SetRadius(0.25);
	tubeHalos->SetNumberOfSides(12);
	//tubeHalos->CappingOn();

	vtkSmartPointer<vtkPolyDataMapper> mapHalo = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapHalo->SetInputConnection( tubeHalos->GetOutputPort() );

	vtkSmartPointer<vtkProperty> haloProperty = vtkSmartPointer<vtkProperty>::New();
	haloProperty->SetRepresentationToWireframe();
	haloProperty->FrontfaceCullingOn();
	haloProperty->SetColor(0,0,0);
	haloProperty->SetLineWidth(m_nHaloWidth);
	//haloProperty->SetInterpolationToGouraud();

	vtkSmartPointer<vtkActor> haloActor = vtkSmartPointer<vtkActor>::New();
	haloActor->SetMapper( mapHalo );
	haloActor->SetProperty( haloProperty );

	m_render->AddViewProp ( haloActor );
}

void CLegiMainWindow::__uniformTubeRendering()
{
	vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();
	//vtkSmartPointer<vtkDepthSortPoints> linesort = vtkSmartPointer<vtkDepthSortPoints>::New();
	vtkDepthSortPoints* linesort = m_linesort;
	//vtkSmartPointer<vtkDepthSortPoints> dsort = vtkSmartPointer<vtkDepthSortPoints>::New();
	vtkDepthSortPoints* dsort = m_dsort;

	//vtkSmartPointer<vtkTubeFilterEx> streamTube = vtkSmartPointer<vtkTubeFilterEx>::New();
	vtkTubeFilter *streamTube = m_streamtube;

	//m_pstlineModel->GetOutput()->GetPointData()->GetArray("FA COLORS", __i)->PrintSelf(cout, vtkIndent());

	//linesort->GetOutput()->GetPointData()->AddArray( m_pstlineModel->GetOutput()->GetPointData()->GetArray("FA COLORS") );
	//
	m_fiberSelector->SetInput( m_pstlineModel->GetOutput() );
	m_fiberSelector->SetInputArrayToProcess(0,0,0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "ClassId");

	m_faSelector->SetInput( m_fiberSelector->GetOutput() );

	m_faSelector->SetInputArrayToProcess(0,0,0, vtkDataObject::FIELD_ASSOCIATION_CELLS, "TubeFA");

	vtkSmartPointer<vtkGeometryFilter> transfilter = vtkSmartPointer<vtkGeometryFilter>::New();
	//vtkSmartPointer<vtkPolyDataAlgorithm> transfilter = vtkSmartPointer<vtkPolyDataAlgorithm>::New();
	transfilter->SetInput ( m_faSelector->GetOutput() );

	if ( m_bDepthSize ) {
		linesort->SetInput( m_pstlineModel->GetOutput() );
		//linesort->SetInput( transfilter->GetOutput() );
		linesort->SetDirectionToBackToFront();
		linesort->SetVector(0,0,1);
		linesort->SetCamera( m_camera );
		linesort->SortScalarsOn();
		linesort->Update();
		linesort->GetOutput()->ReleaseDataFlagOn();

		/*
		int __i = 2;
		m_pstlineModel->GetOutput()->GetPointData()->GetArray("FA COLORS", __i)->PrintSelf(cout, vtkIndent());
		*/

		//linesort->GetOutput()->GetPointData()->SetActiveScalars("FA COLORS");

		/* merge FA color mapping with transparency mapping */
		/*
		if ( m_bDepthTransparency ) {
			vtkDataArray* da = vtkDataArray::SafeDownCast( linesort->GetOutput()->GetPointData()->GetArray("FA COLORS", __i) );
			vtkIdType szTotal = da->GetNumberOfTuples();
			for (vtkIdType idx = 0; idx < szTotal; idx++) {
				da->InsertComponent( idx, 3, int( (idx+1)* abs(m_transparency[1]-m_transparency[0])/szTotal * 255) );
			}
		}
		*/

		/*
		if ( m_controller->GetLocalProcessId() == 1 ) {
			m_pstlineModel->GetOutput()->GetPointData()->GetArray("FA COLORS", __i)->PrintSelf(cout, vtkIndent());
			m_pstlineModel->GetOutput()->GetPointData()->GetArray("Linear Anisotropy", __i)->PrintSelf(cout, vtkIndent());
			m_pstlineModel->GetOutput()->GetPointData()->GetArray("sort scalars", __i)->PrintSelf(cout, vtkIndent());
		}
		*/

		//streamTube->SetInputConnection( linesort->GetOutputPort() );

		//streamTube->SetInput( m_pstlineModel->GetOutput() );
		streamTube->SetInput( transfilter->GetOutput() );

		/*
		streamTube->SetRadius(0.03);
		streamTube->SetRadiusFactor( 20 );
		*/
		streamTube->SetRadius(m_dptsize.size);
		streamTube->SetRadiusFactor(m_dptsize.scale);
		streamTube->SetVaryRadiusToVaryRadiusByScalar();
	}
	else if ( m_bSizeByFA ) { 
		// tube radius encoded by FA value
		int idx;
		if ( NULL == m_pstlineModel->GetOutput()->GetPointData()->GetArray("Linear Anisotropy", idx) ) {
			cerr << "FATAL: scalar array of Linear Anisotropy not found. \n";
			return;
		}
		m_pstlineModel->GetOutput()->GetPointData()->SetActiveAttribute(idx, vtkDataSetAttributes::SCALARS);
			
		streamTube->SetInput( transfilter->GetOutput() );
		streamTube->SetRadius(m_dptsize.size);
		streamTube->SetRadiusFactor(m_dptsize.scale);
		streamTube->SetVaryRadiusToVaryRadiusByScalar();
	}
	else {
		//streamTube->SetInput( m_pstlineModel->GetOutput() );
		streamTube->SetInput( transfilter->GetOutput() );
		streamTube->SetRadius(m_fRadius);
		streamTube->SetVaryRadiusToVaryRadiusOff();
		//streamTube->SetRadius(m_dptsize.size);
	}

	/*
   	if (!m_pstlineModel->GetOutput()->GetPointData()->GetArray("FA COLORS")) {
		cout << "FA COLORS not found.\n";
	}
	*/

	streamTube->SetNumberOfSides(m_lod);
	streamTube->SetCapping( m_bCapping );
	streamTube->Update();

	//cout << "# tubes: " << streamTube->GetOutput()->GetNumberOfCells() << "\n";

	//vtkSmartPointer<vtkPolyDataMapper> mapStreamTube = vtkSmartPointer<vtkPolyDataMapper>::New();
	vtkPolyDataMapper* mapStreamTube = m_bInit? vtkPolyDataMapper::SafeDownCast(m_actor->GetMapper()) :  vtkPolyDataMapper::New();

	mapStreamTube->GlobalImmediateModeRenderingOn();

	//vtkSmartPointer<vtkProperty> tubeProperty = vtkSmartPointer<vtkProperty>::New();
	vtkProperty* tubeProperty = m_bInit? m_actor->GetProperty() : vtkProperty::New();

	/*
	vtkPolyDataMapper* mapStreamTube = vtkPolyDataMapper::SafeDownCast( m_actor->GetMapper() );
	vtkProperty* tubeProperty = m_actor->GetProperty();
	*/

	/*
	cout << "in here...\n";
	*/

	if ( m_bDepthHalo ) {
		__uniform_halos();
	}

	/*
	int numPoints = streamTube->GetOutput()->GetNumberOfPoints();
	int tnumPoints = 0;
	if ( this->m_controller->AllReduce(&numPoints, &tnumPoints, 1, vtkCommunicator::SUM_OP) == 0 ) {
		cout << "Reducing number of depth values from all computing nodes to Root process failed.\n";
		return;
	}
	*/

	vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
	//vtkLookupTable*  lut = vtkLookupTable::SafeDownCast( mapStreamTube->GetLookupTable() );

	if ( m_bDepthTransparency || m_bDepthColorLAB || m_bDepthValue ) {
		dsort->SetInputConnection( streamTube->GetOutputPort() );
		dsort->SetDirectionToBackToFront();
		//dsort->SetDirectionToFrontToBack();
		//dsort->SetDepthSortModeToParametricCenter();
		//dsort->SetDirectionToSpecifiedVector();
		dsort->SetVector(0,0,1);
		//dsort->SetOrigin(0,0,0);
		dsort->SetCamera( m_camera );
		dsort->SortScalarsOn();
		dsort->Update();
		dsort->GetOutput()->ReleaseDataFlagOn();

		//mapStreamTube->SetInputConnection(dsort->GetOutputPort());
		mapStreamTube->SetInputConnection(streamTube->GetOutputPort());

		if ( m_bDepthColorLAB ) {
			vtkIdType total = m_labColors->GetNumberOfPoints();
			lut->SetNumberOfTableValues( total  );

			vtkSmartPointer<vtkLookupTable> lut2 = vtkSmartPointer<vtkLookupTable>::New();
			lut2->SetNumberOfTableValues( total  );

			double labValue[4];
			for (vtkIdType idx = 0; idx < total; idx++ ) {
				labValue[3] = m_bDepthTransparency?((idx+1)*1.0/total):1.0;
				m_labColors->GetPoint( idx, labValue );
				//cout << labValue[0] << "," << labValue[1] << "," << labValue[2] << "\n";
				lut->SetTableValue(idx, labValue);
				lut2->SetTableValue(total-1-idx, labValue);
			}

			if ( m_bCurveMapping ) {
				lut->SetScaleToLog10();
			}
			else {
				lut->SetScaleToLinear();
			}
			/*
			lut->SetHueRange( 0.0, 0.0 );
			lut->SetSaturationRange(0.0, 0.0);
			lut->SetValueRange( 0.0, 1.0);
			*/

			/*
			lut->SetHueRange( m_dptcolor.hue[0], m_dptcolor.hue[1] );
			lut->SetSaturationRange(m_dptcolor.satu[0], m_dptcolor.satu[1]);
			lut->SetValueRange( m_dptcolor.value[0], m_dptcolor.value[1] );
			*/
			//lut->SetAlpha( 0.5 );
			/*
			lut->SetTableRange(0,1);
			m_colorbar->SetLookupTable( lut );
			m_colorbar->SetNumberOfLabels( 5 );
			m_colorbar->SetTitle ( "Lab" );
			m_colorbar->SetMaximumWidthInPixels(60);
			m_colorbar->SetMaximumHeightInPixels(300);
			m_colorbar->SetLabelFormat("%.2f");

			m_render->AddActor2D( m_colorbar );
			*/
			lut2->SetRange(0.0,40.0);
			lut2->SetRange(0.0,40.0);

			lut2->SetScaleToLinear();
			m_colorbar->SetLookupTable( lut2 );
			m_colorbar->SetNumberOfLabels( 5 );
			m_colorbar->SetTitle ( "unit:mm" );
			m_colorbar->SetMaximumWidthInPixels(35);
			m_colorbar->SetMaximumHeightInPixels(200);
			/*
			m_colorbar->SetMaximumWidthInPixels(800);
			m_colorbar->SetMaximumHeightInPixels(60);
			*/
			//m_colorbar->SetLabelFormat("%-#1.1g");
			m_colorbar->SetLabelFormat("%2.0f");
			//m_colorbar->SetOrientationToHorizontal();
			m_colorbar->SetDisplayPosition(10,10);

			m_colorbar->GetLabelTextProperty()->SetJustificationToLeft();
			m_colorbar->GetLabelTextProperty()->SetFontSize(10);
			m_colorbar->GetLabelTextProperty()->SetFontFamilyToTimes();

			m_colorbar->GetTitleTextProperty()->SetJustificationToRight();
			m_colorbar->GetTitleTextProperty()->SetFontSize(30);

			QRect parentRect = renderView->geometry();
			m_colorbar->SetDisplayPosition( parentRect.x() + parentRect.width() - 45,
					parentRect.y() + parentRect.height() - 200 );

			m_render->AddActor2D( m_colorbar );
		}
		else {
			lut->SetHueRange( 0.0, 0.0 );
			lut->SetSaturationRange(0.0, 0.0);
			lut->SetValueRange( 1.0, 1.0);
			lut->SetAlphaRange( 1.0, 1.0 );
		}

		if ( m_bDepthValue ) {
			lut->SetHueRange( 0.6, 0.6 );
			lut->SetSaturationRange(0.7, 0.7);
			lut->SetValueRange( m_value[0], m_value[1] );
		}
		else {
			lut->SetValueRange( 1.0, 1.0);
		}

		if ( m_bDepthTransparency ) {
			//lut->SetAlphaRange( 0.01, 1.0 );
			lut->SetAlphaRange( m_transparency[0], m_transparency[1] );
		}
		else {
			lut->SetAlphaRange( 1.0, 1.0 );
		}

		mapStreamTube->SetLookupTable( lut );

		//mapStreamTube->SetScalarRange( dsort->GetOutput()->GetCellData()->GetScalars()->GetRange());
		//mapStreamTube->SetScalarRange(0, dsort->GetOutput()->GetNumberOfCells());
		//mapStreamTube->SetScalarRange(0, dsort->GetOutput()->GetNumberOfPoints());
		mapStreamTube->SetScalarRange(0, streamTube->GetOutput()->GetNumberOfPoints());

		//streamTube->GetOutput()->GetCellData()->GetScalars()->PrintSelf(cout, vtkIndent());
		//mapStreamTube->UseLookupTableScalarRangeOn();
		mapStreamTube->ScalarVisibilityOn();

		//tubeProperty->SetColor(1,1,1);
		//tubeProperty->SetRepresentationToWireframe();
		//tubeProperty->SetColor(0,0,0);
		//tubeProperty->SetLineWidth(2.0);
	}
	else {
		//cout << "Fa color set.\n";
		mapStreamTube->SetInputConnection(streamTube->GetOutputPort());
		mapStreamTube->ScalarVisibilityOff();
	}

	if ( m_bDepthColor ) /* If FA color is applied */
	{
		mapStreamTube->SetScalarModeToUsePointFieldData();
		mapStreamTube->SelectColorArray("FA COLORS");
		mapStreamTube->ScalarVisibilityOn();

		//mapStreamTube->SetLookupTable ( m_pstlineModel->GetColorTable() );
		//mapStreamTube->SetScalarRange(0, streamTube->GetOutput()->GetNumberOfPoints() );

		/*
		vtkSmartPointer<vtkPolyDataMapper2D> mm = vtkSmartPointer<vtkPolyDataMapper2D>::New();
		mm->SetInput( m_pstlineModel->GetOutput() );
		m_colorbar->SetMapper( mm );
		*/

		m_colorbar->SetLookupTable( m_pstlineModel->GetColorTable() );
		//m_colorbar->SetLookupTable( mapStreamTube->GetLookupTable() );
		m_colorbar->SetNumberOfLabels( 5 );
		m_colorbar->SetTitle ( "FA" );
		m_colorbar->SetMaximumWidthInPixels(40);
		m_colorbar->SetMaximumHeightInPixels(300);
		m_colorbar->SetLabelFormat("%.2f");
		m_colorbar->GetLabelTextProperty()->SetJustificationToRight();
		m_colorbar->GetTitleTextProperty()->SetFontSize(10);
		m_colorbar->GetProperty()->SetDisplayLocationToForeground();

		//QRect parentRect = qApp->desktop()->screenGeometry();
		QRect parentRect = renderView->geometry();
		m_colorbar->SetDisplayPosition( parentRect.x() + parentRect.width() - 45,
										parentRect.y() + parentRect.height() - 300 );

		m_render->AddActor2D( m_colorbar );
	}

	if ( !m_bInit ) {
		m_actor->SetMapper( mapStreamTube );
		m_actor->SetProperty( tubeProperty );
		m_render->SetActiveCamera( m_camera );
		m_render->AddActor( m_actor );

		m_bInit = true;
	}
	/*
	if ( m_bDepthSize ) {
		linesort->SetProp3D( m_actor );
	}
	if ( m_bDepthTransparency ) {
		dsort->SetProp3D( m_actor );
		//streamTubeActor->RotateX( -72 );
	}
	*/

	renderView->update();
	cout << "uniform_rendering finished.\n";

	return;

	vtkSmartPointer<vtkActor> streamTubeActor = vtkSmartPointer<vtkActor>::New();
	streamTubeActor->SetMapper(mapStreamTube);
	streamTubeActor->SetProperty( tubeProperty );

	if ( m_bDepthSize ) {
		linesort->SetProp3D( streamTubeActor );
	}
	if ( m_bDepthTransparency ) {
		dsort->SetProp3D( streamTubeActor );
		//streamTubeActor->RotateX( -72 );
	}

	m_render->SetActiveCamera( camera );
	m_render->AddActor( streamTubeActor );
	m_render->ResetCamera();
}

void CLegiMainWindow::__uniform_halos()
{
	/*
	vtkSmartPointer<vtkDepthSortPoints> linesort = vtkSmartPointer<vtkDepthSortPoints>::New();
	vtkSmartPointer<vtkDepthSortPoints> dsort = vtkSmartPointer<vtkDepthSortPoints>::New();

	vtkSmartPointer<vtkTubeFilterEx> streamTube = vtkSmartPointer<vtkTubeFilterEx>::New();
	*/

	vtkTubeFilter* streamTube = m_streamtube;

	/*
	if ( m_bDepthSize ) {
		linesort->SetInput( m_pstlineModel->GetOutput() );
		linesort->SetDirectionToBackToFront();
		linesort->SetVector(0,0,1);
		linesort->SetCamera( m_camera );
		linesort->SortScalarsOn();
		linesort->Update();
		streamTube->SetInputConnection( linesort->GetOutputPort() );
		streamTube->SetRadius(m_dptsize.size);
		streamTube->SetRadiusFactor(m_dptsize.scale);
		streamTube->SetVaryRadiusToVaryRadiusByScalar();
	}
	else {
		streamTube->SetInput( m_pstlineModel->GetOutput() );
		streamTube->SetRadius(m_fRadius);
	}
	*/

	streamTube->SetNumberOfSides(TUBE_LOD);
	streamTube->SetCapping( m_bCapping );

	vtkPolyDataMapper* mapStreamTube = m_bFirstHalo? vtkPolyDataMapper::New() :
			vtkPolyDataMapper::SafeDownCast( m_haloactor->GetMapper() );
	vtkProperty* tubeProperty = m_bFirstHalo? vtkProperty::New() : m_haloactor->GetProperty();

	tubeProperty->SetRepresentationToWireframe();
	tubeProperty->FrontfaceCullingOn();
	tubeProperty->SetColor(0,0,0);
	tubeProperty->SetLineWidth(m_nHaloWidth);

	mapStreamTube->SetInputConnection(streamTube->GetOutputPort());
	mapStreamTube->ScalarVisibilityOff();

	/*
	if ( m_bDepthTransparency || m_bDepthColor ) {
		dsort->SetInputConnection( streamTube->GetOutputPort() );
		dsort->SetDirectionToBackToFront();
		dsort->SetVector(0,0,1);
		dsort->SetCamera( m_camera );
		dsort->SortScalarsOn();
		dsort->Update();

		mapStreamTube->SetInputConnection(dsort->GetOutputPort());

		vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
		if ( m_bDepthTransparency ) {
			//lut->SetAlphaRange( 0.01, 1.0 );
			lut->SetAlphaRange( m_transparency[0], m_transparency[1] );
			lut->SetValueRange( 1.0, 0.0);
		}
		else {
			lut->SetAlphaRange( 1.0, 1.0 );
			lut->SetValueRange( 0.0, 0.0);
		}

		lut->SetHueRange( 0.0, 0.0 );
		lut->SetSaturationRange(0.0, 0.0);

		mapStreamTube->SetLookupTable( lut );
		mapStreamTube->SetScalarRange(0, dsort->GetOutput()->GetNumberOfPoints());
	}
	else {
		mapStreamTube->SetInputConnection(streamTube->GetOutputPort());
		mapStreamTube->ScalarVisibilityOff();
	}
	*/

	if ( m_bFirstHalo ) {
		m_haloactor->SetMapper( mapStreamTube );
		mapStreamTube->Delete();

		m_haloactor->SetProperty( tubeProperty );
		tubeProperty->Delete();

		m_render->AddActor( m_haloactor );
		m_bFirstHalo = false;
	}

	/*
	if ( m_bDepthSize ) {
		linesort->SetProp3D( m_haloactor );
	}
	if ( m_bDepthTransparency ) {
		dsort->SetProp3D( m_haloactor );
		//streamTubeActor->RotateX( -72 );
	}
	*/

	renderView->update();
}

void CLegiMainWindow::__removeAllActors()
{
	m_render->RemoveAllViewProps();
	vtkActorCollection* allactors = m_render->GetActors();
	vtkActor * actor = allactors->GetNextActor();
	while ( actor ) {
		m_render->RemoveActor( actor );
		actor = allactors->GetNextActor();
	}
	cout << "Numer of Actor in the render after removing all: " << (m_render->VisibleActorCount()) << "\n";
}

void CLegiMainWindow::__removeAllVolumes()
{
	m_render->RemoveAllViewProps();
	vtkVolumeCollection* allvols = m_render->GetVolumes();
	vtkVolume * vol = allvols->GetNextVolume();
	while ( vol ) {
		m_render->RemoveVolume(vol);
		vol = allvols->GetNextVolume();
	}

	cout << "Numer of Volumes in the render after removing all: " << (m_render->VisibleVolumeCount()) << "\n";
}

void CLegiMainWindow::__init_volrender_methods()
{
	for (int idx = VM_START + 1; idx < VM_END; idx++) {
		comboBoxVolRenderMethods->addItem( QString(g_volrender_methods[ idx ]) );
	}
}

void CLegiMainWindow::__init_tf_presets()
{
	for (int idx = VP_START + 1; idx < VP_END; idx++) {
		comboBoxVolRenderPresets->addItem( QString(g_volrender_presets[ idx ]) );
	}
}

void CLegiMainWindow::__add_texture_strokes()
{
	vtkSmartPointer<vtkTubeFilter> streamTube = vtkSmartPointer<vtkTubeFilter>::New();
	streamTube->SetInput( m_pstlineModel->GetOutput() );
	streamTube->SetRadius(0.25);
	streamTube->SetNumberOfSides(12);
	streamTube->SetCapping( m_bCapping );
	streamTube->SetGenerateTCoordsToUseLength();
	streamTube->SetTextureLength(1.0);
	streamTube->SetGenerateTCoordsToUseScalars();

	vtkSmartPointer<vtkTextureMapToCylinder> tmapper = vtkSmartPointer<vtkTextureMapToCylinder>::New();
	//vtkSmartPointer<vtkTextureMapToPlane> tmapper = vtkSmartPointer<vtkTextureMapToPlane>::New();
	//vtkSmartPointer<vtkTextureMapToSphere> tmapper = vtkSmartPointer<vtkTextureMapToSphere>::New();
	tmapper->SetInputConnection( streamTube->GetOutputPort() );

	vtkSmartPointer<vtkTransformTextureCoords> xform = vtkSmartPointer<vtkTransformTextureCoords>::New();
	xform->SetInputConnection(tmapper->GetOutputPort());
	xform->SetScale(10, 10, 10);
	//xform->SetScale(100, 100, 100);
	//xform->FlipROn();
	xform->SetOrigin(0,0,0);


	vtkSmartPointer<vtkTexture> atex = vtkSmartPointer<vtkTexture>::New();
	vtkSmartPointer<vtkPNGReader> png = vtkSmartPointer<vtkPNGReader>::New();
	png->SetFileName("/home/chap/session2.png");
	//png->SetFileName("/home/chap/wait.png");

	/*
	vtkSmartPointer<vtkPolyDataReader> png = vtkSmartPointer<vtkPolyDataReader>::New();
	png->SetFileName("/home/chap/hello.vtk");
	*/


	atex->SetInputConnection( png->GetOutputPort() );
	atex->RepeatOn();
	atex->InterpolateOn();

	vtkSmartPointer<vtkPolyDataMapper> mapStreamTube = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapStreamTube->SetInputConnection(xform->GetOutputPort());
	//mapStreamTube->SetInputConnection(streamTube->GetOutputPort());

	vtkSmartPointer<vtkActor> streamTubeActor = vtkSmartPointer<vtkActor>::New();
	streamTubeActor->SetMapper(mapStreamTube);

	vtkSmartPointer<vtkProperty> tubeProperty = vtkSmartPointer<vtkProperty>::New();
	//tubeProperty->SetRepresentationToWireframe();
	//tubeProperty->SetColor(0.5,0.5,0);
	//tubeProperty->SetLineWidth(2.0);
	streamTubeActor->SetProperty( tubeProperty );
	streamTubeActor->SetTexture( atex );
  
	m_render->AddActor( streamTubeActor );
}

void CLegiMainWindow::__iso_surface(bool outline)
{
	vtkSmartPointer<vtkTubeFilter> streamTube = vtkSmartPointer<vtkTubeFilter>::New();
	streamTube->SetInput( m_pstlineModel->GetOutput() );
	streamTube->SetRadius(0.25);
	streamTube->SetNumberOfSides(12);

	vtkSmartPointer<vtkContourFilter> skinExtractor =
		vtkSmartPointer<vtkContourFilter>::New();
	skinExtractor->SetInputConnection(streamTube->GetOutputPort());
	//skinExtractor->SetInputConnection( m_pImgRender->dicomReader->GetOutputPort() );
	//skinExtractor->SetInputConnection( m_pImgRender->niftiImg->GetOutputPort() );
	skinExtractor->UseScalarTreeOn();
	skinExtractor->ComputeGradientsOn();
	skinExtractor->SetValue(0, -.01);
	//skinExtractor->SetValue(0, 500);
	//skinExtractor->GenerateValues(10, 0, 1000);
	skinExtractor->ComputeNormalsOn();

	vtkSmartPointer<vtkPolyDataNormals> skinNormals =
		vtkSmartPointer<vtkPolyDataNormals>::New();
	skinNormals->SetInputConnection(skinExtractor->GetOutputPort());
	skinNormals->SetFeatureAngle(60.0);

	vtkSmartPointer<vtkPolyDataMapper> skinMapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
	skinMapper->SetInputConnection(skinNormals->GetOutputPort());
	//skinMapper->SetInputConnection(skinExtractor->GetOutputPort());
	skinMapper->ScalarVisibilityOff();

	vtkSmartPointer<vtkActor> skin =
		vtkSmartPointer<vtkActor>::New();
	skin->SetMapper(skinMapper);

	vtkSmartPointer<vtkCamera> aCamera =
		vtkSmartPointer<vtkCamera>::New();
	aCamera->SetViewUp (0, 0, -1);
	aCamera->SetPosition (0, 1, 0);
	aCamera->SetFocalPoint (0, 0, 0);
	aCamera->ComputeViewPlaneNormal();
	aCamera->Azimuth(30.0);
	aCamera->Elevation(30.0);

	if ( outline ) {
		vtkSmartPointer<vtkOutlineFilter> outlineData =
			vtkSmartPointer<vtkOutlineFilter>::New();
		outlineData->SetInputConnection(streamTube->GetOutputPort());

		vtkSmartPointer<vtkPolyDataMapper> mapOutline =
			vtkSmartPointer<vtkPolyDataMapper>::New();
		mapOutline->SetInputConnection(outlineData->GetOutputPort());

		vtkSmartPointer<vtkActor> outline =
			vtkSmartPointer<vtkActor>::New();
		outline->SetMapper(mapOutline);
		outline->GetProperty()->SetColor(0,0,0);


		m_render->AddActor(outline);
	}

	m_render->AddActor(skin);
	/*
	m_render->SetActiveCamera(aCamera);
	m_render->ResetCamera ();
	aCamera->Dolly(1.5);
	*/

	renderView->update();
}

void CLegiMainWindow::__add_axes()
{
	vtkSmartPointer<vtkAnnotatedCubeActor> cube = vtkSmartPointer<vtkAnnotatedCubeActor>::New();
	cube->SetXPlusFaceText ( "A" );
	cube->SetXMinusFaceText( "P" );
	cube->SetYPlusFaceText ( "L" );
	cube->SetYMinusFaceText( "R" );
	cube->SetZPlusFaceText ( "S" );
	cube->SetZMinusFaceText( "I" );
	cube->SetFaceTextScale( 0.65 );

	vtkProperty* property = cube->GetCubeProperty();
	property->SetColor( 0.5, 1, 1 );

	property = cube->GetTextEdgesProperty();
	property->SetLineWidth( 1 );
	property->SetDiffuse( 0 );
	property->SetAmbient( 1 );
	property->SetColor( 0.1800, 0.2800, 0.2300 );

	vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();

	property = cube->GetXPlusFaceProperty();
	property->SetColor(0, 0, 1);
	property->SetInterpolationToFlat();
	property = cube->GetXMinusFaceProperty();
	property->SetColor(0, 0, 1);
	property->SetInterpolationToFlat();
	property = cube->GetYPlusFaceProperty();
	property->SetColor(0, 1, 0);
	property->SetInterpolationToFlat();
	property = cube->GetYMinusFaceProperty();
	property->SetColor(0, 1, 0);
	property->SetInterpolationToFlat();
	property = cube->GetZPlusFaceProperty();
	property->SetColor(1, 0, 0);
	property->SetInterpolationToFlat();
	property = cube->GetZMinusFaceProperty();
	property->SetColor(1, 0, 0);
	property->SetInterpolationToFlat();

	vtkSmartPointer<vtkAxesActor> axes2 = vtkSmartPointer<vtkAxesActor>::New();
	axes2->SetShaftTypeToCylinder();
	axes2->SetXAxisLabelText( "x" );
	axes2->SetYAxisLabelText( "y" );
	axes2->SetZAxisLabelText( "z" );

	axes2->SetTotalLength( 3.5, 3.5, 3.5 );
	axes2->SetCylinderRadius( 0.500 * axes2->GetCylinderRadius() );
	axes2->SetConeRadius    ( 1.025 * axes2->GetConeRadius() );
	axes2->SetSphereRadius  ( 1.500 * axes2->GetSphereRadius() );

	vtkTextProperty* tprop = axes2->GetXAxisCaptionActor2D()-> GetCaptionTextProperty();
	tprop->ItalicOn();
	tprop->ShadowOn();
	tprop->SetFontFamilyToTimes();

	axes2->GetYAxisCaptionActor2D()->GetCaptionTextProperty()->ShallowCopy( tprop );
	axes2->GetZAxisCaptionActor2D()->GetCaptionTextProperty()->ShallowCopy( tprop );

	vtkSmartPointer<vtkPropAssembly> assembly = vtkSmartPointer<vtkPropAssembly>::New();
	assembly->AddPart( axes2 );
	assembly->AddPart( cube );

	// add orientation widget
	m_orientationWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
	m_orientationWidget->SetOutlineColor( 0.9300, 0.5700, 0.1300 );
	m_orientationWidget->SetOrientationMarker( assembly );

	vtkRenderWindowInteractor* iren = renderView->GetRenderWindow()->GetInteractor();
	//iren->PrintSelf( cout, vtkIndent() );
	m_orientationWidget->SetInteractor( iren );

	m_orientationWidget->SetViewport( -0.1, -0.1, 0.3, 0.3 );
	//renderView->GetRenderWindow()->SetNumberOfLayers(2);
	m_orientationWidget->EnabledOn();
	/* the following control can cause parallel version paralysis for now*/
	m_orientationWidget->InteractiveOff();

	// add vtkButtons
	/*
	vtkSmartPointer<vtkRectangularButtonSource> btn = vtkSmartPointer<vtkRectangularButtonSource>::New();
	btn->SetWidth( 20 );
	btn->SetHeight( 10 );

	vtkSmartPointer<vtkPolyDataMapper> bmapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	bmapper->SetInputConnection( btn->GetOutputPort() );

	vtkSmartPointer<vtkActor> bactor = vtkSmartPointer<vtkActor>::New();
	bactor->SetMapper( bmapper );

	m_render->AddActor( bactor );
	*/
}

void CLegiMainWindow::__add_planes()
{
	// add cutting planes
	m_planeX = vtkSmartPointer<vtkPlaneWidget>::New();
	m_planeX->SetInput( m_pstlineModel->GetOutput() );
	m_planeX->NormalToXAxisOn();
	m_planeX->PlaceWidget();
	m_planeX->SetInteractor( renderView->GetRenderWindow()->GetInteractor() );
	m_planeX->SetRepresentationToSurface();
	m_planeX->GetHandleProperty()->SetOpacity(0.0);
	m_planeX->GetPlaneProperty()->SetColor(0.3,0,0);
	m_planeX->GetPlaneProperty()->SetOpacity(0.3);
	m_planeX->On();

	m_planeY = vtkSmartPointer<vtkPlaneWidget>::New();
	m_planeY->SetInput( m_pstlineModel->GetOutput() );
	m_planeY->NormalToYAxisOn();
	/*
	double normal[3];
	m_planeY->GetNormal(normal);
	m_planeY->SetNormal( 0.0,0.0,-1.0 );
	cout << normal[0] << "," << normal[1] << "," << normal[2] << "\n";
	*/
	m_planeY->PlaceWidget();
	m_planeY->SetInteractor( renderView->GetRenderWindow()->GetInteractor() );
	m_planeY->SetRepresentationToSurface();
	m_planeY->GetHandleProperty()->SetOpacity(0.0);
	m_planeY->GetPlaneProperty()->SetColor(0.0,0.3,0);
	m_planeY->GetPlaneProperty()->SetOpacity(0.3);
	m_planeY->On();

	m_planeZ = vtkSmartPointer<vtkPlaneWidget>::New();
	m_planeZ->SetInput( m_pstlineModel->GetOutput() );
	m_planeZ->NormalToZAxisOn();
	m_planeZ->PlaceWidget();
	m_planeZ->SetInteractor( renderView->GetRenderWindow()->GetInteractor() );
	m_planeZ->SetRepresentationToSurface();
	m_planeZ->GetHandleProperty()->SetOpacity(0.0);
	m_planeZ->GetPlaneProperty()->SetColor(0.0,0.0,0.3);
	m_planeZ->GetPlaneProperty()->SetOpacity(0.3);
	m_planeZ->On();

	// add clipping planes from the plane widgets to activate them during interaction / language operations
	vtkPlaneWidgetCallback *callbackX = vtkPlaneWidgetCallback::New();
	callbackX->SetMapper(m_actor->GetMapper());
	callbackX->SetIndex(0);
	m_planeX->AddObserver(vtkCommand::InteractionEvent, callbackX);
	callbackX->Delete();

	vtkPlaneWidgetCallback *callbackY = vtkPlaneWidgetCallback::New();
	callbackY->SetMapper(m_actor->GetMapper());
	callbackY->SetIndex(1);
	m_planeY->AddObserver(vtkCommand::InteractionEvent, callbackY);
	callbackY->Delete();

	vtkPlaneWidgetCallback *callbackZ = vtkPlaneWidgetCallback::New();
	callbackZ->SetMapper(m_actor->GetMapper());
	callbackZ->SetIndex(2);
	m_planeZ->AddObserver(vtkCommand::InteractionEvent, callbackZ);
	callbackZ->Delete();

	// place all the three planes at proper position as their initial locations
	double extent[6];
	m_pstlineModel->GetOutput()->GetBounds(extent);

	double cpt[3];
	m_planeX->GetCenter(cpt);
	cpt[0] -= (extent[1] - extent[0])/2.0;
	m_planeX->SetCenter( cpt );

	m_planeY->GetCenter(cpt);
	cpt[1] += (extent[3] - extent[2])/2.0;
	m_planeY->SetCenter( cpt );

	m_planeZ->GetCenter(cpt);
	cpt[2] -= (extent[5] - extent[4])/2.0;
	m_planeZ->SetCenter( cpt );

	// initialize the three cutting planes
	vtkPlane *plane = vtkPlane::New();
	m_planeX->GetPlane(plane);
	m_actor->GetMapper()->AddClippingPlane(plane);
	plane->Delete();

	plane = vtkPlane::New();
	m_planeY->GetPlane(plane);
	m_actor->GetMapper()->AddClippingPlane(plane);
	plane->Delete();

	plane = vtkPlane::New();
	m_planeZ->GetPlane(plane);
	m_actor->GetMapper()->AddClippingPlane(plane);
	plane->Delete();

}

double CLegiMainWindow::_calBlockAvgFA(double* boxExtent, int & nLineInbox)
{
	//vtkPolyData* tubes = vtkPolyDataMapper::SafeDownCast(m_actor->GetMapper())->GetInput();
	vtkPolyData* tubes = m_pstlineModel->GetOutput();
	vtkCellArray* alllines = tubes->GetLines();
	vtkIdType szTotal = alllines->GetNumberOfCells();
	vtkDataArray* CL = tubes->GetPointData()->GetArray("Linear Anisotropy");
	if ( !CL ) {
		cerr << "Fatal: in _calBlockAvgFA, FA value array not found in polyData of the geometry.\n";
		return .0;
	}
	/*
	cout << "size of lookuptable: "  <<
		vtkLookupTable::SafeDownCast( vtkPolyDataMapper::SafeDownCast(m_actor->GetMapper())->GetLookupTable() )->GetNumberOfTableValues() << "\n";
	cout << "#tubes: " << szTotal << "\n";
	*/
		
	double extent[6];
	double faTotal = .0, ptTotal = 0.0;
	double point[3];
	vtkIdType* line;
	vtkIdType lineCnt = 0, szPts;

	tubes->GetBounds(extent);

	nLineInbox = 0;
	/* if this was not invoked first, GetNextCell, GetCell will both refuse to work */
	alllines->InitTraversal();
	while ( alllines->GetNextCell(szPts, line) ) {
		double npt = 0;
		for (vtkIdType idx = 0; idx < szPts; idx++) {
			tubes->GetPoint( line[idx], point );
			point[0] -= (extent[0] + extent[1])/2.0;
			point[1] -= (extent[2] + extent[3])/2.0;
			point[2] -= (extent[4]+extent[5])/2.0;

			// box inclusion judgement
			if ( point[0] >= boxExtent[0] && point[0] <= boxExtent[1] &&
				 point[1] >= boxExtent[2] && point[1] <= boxExtent[3] &&
				 point[2] >= boxExtent[4] && point[2] <= boxExtent[5] ) {

				faTotal += CL->GetTuple1( line[idx] );
				npt ++;
			}
		}
		ptTotal += npt;
		if ( npt > 0 ) {
			nLineInbox ++;
		}

		lineCnt ++;
	}

	if ( lineCnt != szTotal ) {
		cerr << "Internal data error in _calBlockAvgFA #cell=" << lineCnt << ", but expected num=" << szTotal << "\n";
	}

	if ( ptTotal < 1.0 ) return -1.0;
	return faTotal / ptTotal;
}

void CLegiMainWindow::__load_colormap()
{
	m_labColors = vtkSmartPointer<vtkPoints>::New();
	int ilabTotal = 0;

	this->m_controller->Barrier();

	if ( this->m_controller->GetLocalProcessId() == 0 ) 
	{
		ifstream ifs ("lab.txt");
		if (!ifs.is_open()) {
			cerr << "can not load LAB color values from lab.txt, ignored.\n";
		}

		double labValue[3];
		while (ifs) {
			ifs >> labValue[0] >> labValue[1] >> labValue[2];
			m_labColors->InsertNextPoint( labValue );
			ilabTotal ++;
		}
		cout << m_labColors->GetNumberOfPoints() << " LAB color loaded.\n";

		/*
		this->m_controller->GetCommunicator()->Broadcast(
				(int*)&ilabTotal,
				1,
				0);
		*/

		this->m_controller->GetCommunicator()->Broadcast(
				m_labColors->GetData(),
				0);
	}
	else 
	{
		/*
		this->m_controller->GetCommunicator()->Broadcast(
				(int*)&ilabTotal,
				1,
				0);

		m_labColors->SetDataTypeToDouble();
		m_labColors->SetNumberOfPoints( ilabTotal );
		m_labColors->GetData()->SetNumberOfTuples( ilabTotal );
		m_labColors->GetData()->SetNumberOfComponents(3);
		this->m_controller->GetCommunicator()->BroadcastVoidArray(
				m_labColors->GetVoidPointer(0),
				ilabTotal*3,
				VTK_DOUBLE,
				0);

		cout << m_labColors->GetNumberOfPoints() << " LAB color loaded.\n";
		m_labColors->Squeeze();
		m_labColors->ComputeBounds();
		*/

		this->m_controller->GetCommunicator()->Broadcast(
				m_labColors->GetData(),
				0);

		/*
		double x[3];
		for (vtkIdType id=0; id < 101; id++) {
			m_labColors->GetPoint (id, x);
			cout << x[0] << " " << x[1] << " " << x[2] << "\n";
		}
		*/
	}
}

void CLegiMainWindow::selfRotate(int range)
{
	float step = (range==0)?5:(360/range);
	if ( range == 0 ) range = 0xffffff;

	for (int i=1;i<=range;i++) {
		m_camera->Azimuth(step);
		//renderView->GetRenderWindow()->Render();
		//m_render->GetActiveCamera()->Azimuth(360/range);
		//onButtonApply();
		/*
		m_render->GetActiveCamera()->Azimuth(360/range);
		//this->m_controller->TriggerRMIOnAllChildren(RENDER_RMI_MAPPING_UPDATE);
		//vtkLegiInteractorStyle::SafeDownCast( renderView->GetInteractor()->GetInteractorStyle() )->updateDataPipeline();


		m_dsort->Update();
		m_actor->GetMapper()->GetInput()->Modified();
		renderView->GetInteractor()->InvokeEvent( vtkCommand::StartEvent, NULL);
		renderView->GetInteractor()->InvokeEvent( vtkCommand::MouseMoveEvent, NULL);
		renderView->GetInteractor()->InvokeEvent( vtkCommand::EndEvent, NULL);
		*/
		renderView->GetInteractor()->LeftButtonPressEvent();
		renderView->GetInteractor()->MouseMoveEvent();

		//this->EventCallbackCommand->AbortFlagOn();


		//renderView->GetInteractor()->ExitEvent();
		//renderView->GetInteractor()->ExitCallback();

		//vtkLegiInteractorStyle::SafeDownCast( renderView->GetRenderWindow()->GetInteractor()->GetInteractorStyle() )->InvokeEvent( vtkCommand::MouseMoveEvent, NULL);
	}
	renderView->GetInteractor()->LeftButtonReleaseEvent();
}

int CLegiMainWindow::__execLine(const QString&  curline)
{
	if ( curline.isEmpty() ) {
		return 0;
	}

	QRegExp wssep("\\s+");
	QString term = curline.section(wssep, 0, 0);
	QString curterm = term.toUpper();
	int ret = -1;

	if ( "SELECT" == curterm || "FIND" == curterm ) {
		ret = __scenario_explore(curline);
	}
	else if ( "UPDATE" == curterm ) {
		ret = __scenario_generate(curline);
		if ( 0 == ret ) {
		}
	}
	else if ( "MSELECT" == curterm ) {
		ret = __scenario_compare(curline);
	}
	else if ( "MEASURE" == curterm ) {
		ret = __scenario_measure(curline);
	}
	else {
		//QMessageBox::critical(this, "Unrecognized type of clause starting with:", term);
		QString errMsg("Unrecognized type of clause starting with: ");
		__debugOutput( errMsg + term );
		return -1;
	}

	return ret;
}

int CLegiMainWindow::__getFiberBundleIdx(const QString& bundleName)
{
	static const char* const knownBundles[] = {"CST", "CG", "CC", "IFO", "ILF"};

	int ret = std::find( knownBundles, knownBundles + ARRAY_SIZE(knownBundles), bundleName ) - knownBundles;

	if ( ARRAY_SIZE(knownBundles) == ret ) {
		return -1;
	}

	return ret + 1;
}

/*
 * @return: 
 * 0: success
 * -1: syntactic error
 * -2: lexical error, i.e. unrecognizable symbol
 */
#if 0
int CLegiMainWindow::__scenario_explore(const QString&  curline)
{
	QRegExp regex("\\s+");
	//regex.setPattern("\\b\"\\b");
	QStringList allterms = curline.split(regex);

	// there are at least three terms in the "exploratory" clause 
	if ( allterms.size() < 3 ) {
		// syntactic error
		return -1;
	}
	int pos = 1;

	if ( "SELECT" == allterms[0].toUpper() ) {

		/* 1: retrieve the target */

		if ( "ALL" == allterms[pos].toUpper() ) {
			m_fiberSelector->ThresholdBetween(1, 5);
			pos ++;
		}
		else if ( "IN" == allterms[pos].toUpper() ) {
		}
		else { // indicate a fiber bundle name embraced by a couple of quotes
			if ( !allterms[pos].startsWith("\"") || !allterms[pos].endsWith("\"") ) {
				// syntax error
				cerr << "here 1!\n";
				return -1;
			}

			allterms[pos].remove('"');

			int bidx = __getFiberBundleIdx( allterms[pos].toUpper() );
			if ( -1 == bidx ) {
				// unrecognizable symbol for bundle name
				cerr << "here 2!\n";
				return -2;
			}

			// execute this term - fiber bundle filter
			m_fiberSelector->ThresholdBetween(bidx, bidx);

			pos ++;
		}

		/* 2: retrieve the spatial relation indicator */
		// we expect the preposition IN or OUT now
		bool bIn = true;
		if ( "IN" == allterms[pos].toUpper() ) {
			bIn = true;
		}
		else if ("OUT" == allterms[pos].toUpper() ) {
			bIn = false;
		}
		else {
			cerr << "here 3!\n";
			// syntactic error
			return -1;
		}

		pos ++;

		/* 3: retrieve the region */
		if ( !allterms[pos].startsWith("\"") || !allterms[pos].endsWith("\"") ) {
			cerr << "here 4!\n";
			// syntax error
			return -1;
		}
		allterms[pos].remove('"');

		// to do here: select one of many, if any, loaded data sets, taking into account the value of "bIn"
		{
			pos ++;
		}

		// clause might have been consumed now
		if ( pos >= allterms.size() ) {
			return 0;
		}

		/* 4: deal with conditional expression */
		if ( "WHERE" != allterms[pos].toUpper() ) {
			cerr << "here 5!\n";
			// syntax error
			return -1;
		}
		pos ++;

		if ( pos >= allterms.size() ) {
			cerr << "here 6!\n";
			// syntax error
			return -1;
		}

		/* 5: retrieve the conditional expression itself */
		QString condexp = curline.mid( curline.indexOf( allterms[pos-1] ) + allterms[pos-1].count() + 1 );
		allterms[pos] = condexp;

		if ( !allterms[pos].startsWith("\"") || !allterms[pos].endsWith("\"") ) {
			//cerr << "here 7 : " << allterms[pos].toStdString() << "\n";
			__debugOutput(allterms[pos] + " ---> Syntax Error", DL_WARNING );
			// syntax error
			return -1;
		}
		allterms[pos].remove('"');

		return __parseCondExp( allterms[pos] );
	}
	else if ( "FIND" == allterms[0].toUpper() ) {
	}
	else {
		// fatal impossible program internal error
		return -1;
	}

	return 0;
}
#endif
int CLegiMainWindow::__scenario_explore(const QString&  curline)
{
	QStringList allterms = curline.split("\"", QString::SkipEmptyParts);
	for ( int i = 0; i < allterms.size(); i++ ) {
		allterms[i] = allterms[i].trimmed();
		//cout << i << " : " << allterms[i].toStdString() << "\n";
	}

	// there are at least two terms in the "exploratory" clause 
	if ( allterms.size() < 2 ) {
		// syntactic error
		__debugOutput(curline + ": incomplete statement", DL_WARNING);
		return -1;
	}
	int pos = 0;
	bool bIn = true;

	if ( "SELECT" == allterms[pos].toUpper() ) {
		pos ++;

		/* 1: deal with conditional expression */
		QString condexp = allterms[pos];

		if ( 0 != __parseCondExp( condexp ) ) {
			//return -1;
			goto totarget;
		}

		pos ++;

		// clause might have been consumed now
		if ( pos >= allterms.size() ) {
			return 0;
		}

		/* 2: preposition expected */
		/* 2: retrieve the spatial relation indicator */
		// we expect the preposition IN or OUT now
		if ( "IN" == allterms[pos].toUpper() ) {
			bIn = true;
		}
		else if ("OUT" == allterms[pos].toUpper() ) {
			bIn = false;
		}
		else {
			__debugOutput(curline + ": prepositional keyword IN/OUT missed", DL_FATAL);
			// syntactic error
			return -1;
		}

		/*
		if ( "IN" != allterms[pos].toUpper() ) {
			return -1;
		}
		*/
		pos ++;

		if ( pos >= allterms.size() ) {
			__debugOutput(curline + ": target expected but missed.", DL_WARNING );
			return -1;
		}

totarget:
		/* 3: retrieve the target */

		if ( "ALL" == allterms[pos].toUpper() ) {
			m_fiberSelector->ThresholdBetween(1, 5);
			pos ++;
		}
		else { // indicate a fiber bundle name embraced by a couple of quotes
			int bidx = __getFiberBundleIdx( allterms[pos].toUpper() );
			if ( -1 == bidx ) {
				// unrecognizable symbol for bundle name
				__debugOutput(allterms[pos] + ": unrecognizable bundle", DL_WARNING );
				cerr << "here 2!\n";
				return -2;
			}

			// execute this term - fiber bundle filter
			/*
			m_fiberSelector->ThresholdBetween(bidx, bidx);
			*/
			m_fiberSelector->ThresholdByMatch();
			m_fiberSelector->addCandidate( bidx );

			pos ++;
		}
	}
	else if ( "FIND" == allterms[0].toUpper() ) {
		QStringList exp = curline.split("=");
		m_strVarTable[ exp[0].trimmed().toStdString() ] = exp[1].trimmed().toStdString();
	}
	else {
		// fatal impossible program internal error
		__debugOutput(allterms[pos] + ": internal error", DL_FATAL);
		return -1;
	}

	return 0;
}

int CLegiMainWindow::__scenario_generate(const QString&  curline)
{
	QRegExp regex("\\s+");
	QStringList allterms = curline.split(regex);
	if ( allterms.size() < 2 ) {
		// syntactic error
		return -1;
	}

	int pos = 1;

	if ( "UPDATE" != allterms[0].toUpper() ) {
		// fatal impossible program internal error
		return -1;
	}

	/* 1: retrieve the target */
	if ( "ALL" == allterms[pos].toUpper() ) {
		m_fiberSelector->ThresholdBetween(1, 5);
		pos ++;
	}
	else 
	if ( "RESET" == allterms[pos].toUpper() ) {
		m_fiberSelector->ThresholdBetween(1, 5);
		m_fiberSelector->clearCandidates();
		m_faSelector->ThresholdBetween(0, 1);
		return 0;
	}
	else if ("DEFAULT" == allterms[pos].toUpper() ) {
		m_bDepthTransparency = m_bDepthSize = m_bDepthColor = m_bDepthValue = m_bDepthColorLAB = false;
		return 0;
	}

	// there are at least four terms in the update clause if it is not "UPDATE RESET"
	if ( allterms.size() < 4 ) {
		// syntactic error
		return -1;
	}

	int curTag = VN_NONE;

	/* 2: retrieve the variable 1, the function */
	if ( "DEPTH" == allterms[pos].toUpper() ) {
		pos ++;

		if ( "BY" != allterms[pos].toUpper() ) {
			// syntactic error
			return -1;
		}
		pos ++;

		/* 3: retrieve the variable 2 */
		if ( "size" == allterms[pos].toLower() ) {
			m_bDepthSize = true;
			curTag = VN_SIZE;
		}
		else
		if ( "color" == allterms[pos].toLower() ) {
			m_bDepthColorLAB = true;
			curTag = VN_COLOR;
		}
		else 
		if ( "value" == allterms[pos].toLower() ) {
			m_bDepthValue = true;
			curTag = VN_VALUE;
		}
		else 
		if ( "transparency" == allterms[pos].toLower() ) {
			m_bDepthTransparency = true;
			curTag = VN_TRANSPARENCY;
		}
		else {
			// syntactic error
			return -1;
		}
		pos ++;
	}
	else if ( "COLOR" == allterms[pos].toUpper() ) {
		pos ++;

		if ( "BY" != allterms[pos].toUpper() ) {
			// syntactic error
			return -1;
		}
		pos ++;

		/* 3: retrieve the variable 2 */
		if ( "FA" == allterms[pos].toUpper() ) {
			m_bDepthColor = true;
			cerr << "update done here FA" << endl;
		}
		else
		if ( "DIRECTION" == allterms[pos].toUpper() ) {
		}
		else {
			// syntactic error
			return -1;
		}
		pos ++;

		// for this clause, nothing more should be expected
		if ( pos < allterms.size() ) {
			// syntactic error
			return -1;
		}

		// stop here
		return 0;
	}
	else if ( "SIZE" == allterms[pos].toUpper() ) {
		pos ++;

		if ( "BY" != allterms[pos].toUpper() ) {
			// syntactic error
			return -1;
		}
		pos ++;

		/* 3: retrieve the variable 2 */
		if ( "FA" == allterms[pos].toUpper() ) {
			m_bSizeByFA = true;
			curTag = VN_SIZE;
			cerr << "update done here size by FA" << endl;
		}
		else if ( "LA" == allterms[pos].toUpper() ) {
		}
		else {
			// syntactic error
			return -1;
		}
		pos ++;

		// for this clause, nothing more should be expected
		if ( pos < allterms.size() ) {
			// syntactic error
			return -1;
		}

		// stop here
		return 0;
	}
	else { // unrecognizable term here
		// syntactic error
		return -1;
	}

	// parameter list is optional
	if ( pos >= allterms.size() ) {
		return 0;
	}

	// othewise, next term must be "WITH"
	if ( "WITH" != allterms[pos].toUpper() ) {
		// syntactic error
		return -1;
	}
	pos ++;

	/* 4: retrieve the parameter list */
	QString paralist = curline.mid( curline.indexOf( allterms[pos-1] ) + allterms[pos-1].count() + 1 );
	allterms[pos] = paralist;

	return __parseParaList( allterms[pos], curTag );
}

int CLegiMainWindow::__scenario_compare(const QString&  curline)
{
	return 0;
}

int CLegiMainWindow::__scenario_measure(const QString&  curline)
{
	return 0;
}

int CLegiMainWindow::__parseCondExp(const QString& exp)
{
	// the two bounds of the expected quantity filter
	double ub = 0, lb = 0;
	QStringList condexp = exp.split( QRegExp("\\s+") ); 
	if ( condexp.size() < 2 ) {
		cerr << "here 8 " << "\n";
		// syntax error
		return -1;
	}

	int pos = 0;
	if ( "FA" == condexp[pos].toUpper() ) {
		if ( condexp.size() < 3 ) {
			// syntax error
			return -1;
		}

		pos ++;

		// retrieve the comparative operators
		if ( ">=" == condexp[pos] || ">" == condexp[pos] ) {
			m_faSelector->ThresholdByUpper( ub = condexp[pos+1].toDouble() );
			cout << "done here 1\n";
		}
		else if ( "<=" == condexp[pos] || "<" == condexp[pos] ) {
			m_faSelector->ThresholdByLower( lb = condexp[pos+1].toDouble() );
			cout << "done here 2\n";
		}
		else if ( "==" == condexp[pos] ) {
			m_faSelector->ThresholdBetween( lb = condexp[pos+1].toDouble(), ub = condexp[pos+1].toDouble() );
			cout << "done here 3\n";
		}
		else if ( "in" == condexp[pos].toLower() ) {
			condexp[pos+1].remove('[');
			condexp[pos+1].remove(']');

			QStringList bounds = condexp[pos+1].split(',');
			if ( bounds.size() < 2 ) {
				cerr << "here 9 " << "\n";
				// syntax error
				return -1;
			}

			m_faSelector->ThresholdBetween( lb = bounds[0].toDouble(), ub = bounds[1].toDouble() );
			cout << "done here 4\n";
		}
		else {
			cerr << "here 10 " << "\n";
			// syntax error
			return -1;
		}
	}
	else if ( -1 != exp.indexOf( QRegExp("axial|coronal|sagittal", Qt::CaseInsensitive), 0 ) ) {
		return	__spatialOps( exp );
	}
	else {
		// to do: for filtering by other quantities such as "LA", "ADC", "RA", etc.
	}

	return 0;
}

int CLegiMainWindow::__parseParaList(const QString& curline, int curTag)
{
	QStringList localTerms = curline.split(',');
	if ( localTerms.size() < 2 ) {
		// syntactic error
		return -1;
	}

	switch ( curTag ) {
		case VN_SIZE:
			{
				m_dptsize.size = localTerms[0].toFloat();
				m_dptsize.scale = localTerms[1].toFloat();
			}
			break;
		case VN_COLOR:
			break;
		case VN_TRANSPARENCY:
			{
				m_transparency[0] = localTerms[0].toFloat();
				m_transparency[1] = localTerms[1].toFloat();
			}
			break;
		case VN_VALUE:
			{
				m_value[0] = localTerms[0].toFloat();
				m_value[1] = localTerms[1].toFloat();
			}
			break;
		case VN_HALO:
			m_nHaloWidth = localTerms[0].toInt();
			break;
		default:
			return -1;
	}

	return 0;
}

void CLegiMainWindow::__setDebugLevel(int debugLevel)
{
	QColor textcolor, bkgcolor;
	switch ( debugLevel ) {
		case DL_LOG:
			textcolor = Qt::gray;
			bkgcolor = Qt::white;
			break;
		case DL_WARNING:
			textcolor = Qt::cyan;
			bkgcolor = Qt::white;
			break;
		case DL_FATAL:
			textcolor = Qt::red;
			bkgcolor = Qt::white;
			break;
		case DL_EVAL: // for metric computation results
			textcolor = Qt::yellow;
			bkgcolor = Qt::lightGray;
			textEditDebug->setFontUnderline(true);
			break;
		case DL_NONE:
		default:
			textcolor = Qt::black;
			bkgcolor = Qt::white;
			break;
	}

	textEditDebug->setTextColor( textcolor );
	textEditDebug->setTextBackgroundColor( bkgcolor );
}

void CLegiMainWindow::__debugOutput(const QString& msg, int debugLevel)
{
	__setDebugLevel( debugLevel );

	// force a newline
	if ( !msg.endsWith('\n') ) {
		textEditDebug->insertPlainText( msg + "\n" );
	}
	else {
		textEditDebug->insertPlainText( msg + "\n" );
	}
}

int CLegiMainWindow::__spatialOps(const QString& sop)
{
	QStringList terms = sop.split(QRegExp("\\s+"));

	// for relative spatial operations, at least two terms are expected.
	if ( terms.size() < 2 ) {
		__debugOutput(sop + ": incomplete relative spatial operations", DL_WARNING);
		return -1;
	}

	if ( terms[1][0] != '+' && terms[1][0] != '-' ) {
		__debugOutput(sop + ": relative operator +/- expected.");
		return -1;
	}

	bool bMinus = terms[1][0] == '-';
	double movement = terms[1].mid(1).toDouble();
	cout << "movement = " << ( bMinus?'-':'+' ) << movement << "\n";

	vtkPlaneWidget* planeWidget = NULL;
	double cpt[3];
	int idx = 0;
	if ( "axial" == terms[0].toLower() ) {
		planeWidget = m_planeZ;
		idx = 2;
	}
	else if ( "coronal" == terms[0].toLower() ) {
		planeWidget = m_planeY;
		idx = 1;
	}
	else if ( "sagittal" == terms[0].toLower() ) {
		planeWidget = m_planeX;
		idx = 0;
	}

	planeWidget->GetCenter(cpt);
	if ( bMinus ) {
		cpt[idx] -= movement;
	}
	else {
		cpt[idx] += movement;
	}
	planeWidget->SetCenter( cpt );
	planeWidget->UpdatePlacement();
	planeWidget->InvokeEvent(vtkCommand::InteractionEvent, NULL);

	return 0;
}

/* sts=8 ts=8 sw=80 tw=8 */

