// ----------------------------------------------------------------------------
// engine_helper.cpp : common structure and class serving the SVL engine
//
// Creation : Mar. 23rd 2012
//
// Copyright(C) 2012-2013 Haipeng Cai
//
// ----------------------------------------------------------------------------
#include "engine_helper.h"

#include <vtkActor.h>

using namespace std;

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
	this->pipelines = NULL;
}

vtkPlaneWidgetCallback* vtkPlaneWidgetCallback::New()
{
	return new vtkPlaneWidgetCallback; 
}

void vtkPlaneWidgetCallback::Execute(vtkObject *caller, unsigned long, void*)
{
	vtkPlaneWidget *widget = reinterpret_cast<vtkPlaneWidget*>(caller);
	if ( ! this->pipelines ) return;

	for (size_t i = 0; i < this->pipelines->size(); i++) {
		this->Mapper = this->pipelines->at(i).m_actor->GetMapper();

		if (this->Mapper) {
			vtkPlane *plane = vtkPlane::New();
			widget->GetPlane(plane);
			/*
			this->Mapper->RemoveClippingPlane( this->Mapper->GetClippingPlanes()->GetItem( this->GetIndex() ) );
			this->Mapper->AddClippingPlane(plane);
			*/
			if ( this->Mapper->GetClippingPlanes() && this->Mapper->GetClippingPlanes()->GetNumberOfItems() - 1 >= this->GetIndex() ) {
				this->Mapper->GetClippingPlanes()->ReplaceItem( this->GetIndex(), plane );
			}
			else {
				this->Mapper->AddClippingPlane(plane);
			}
			plane->Delete();
		}
	}
}

void vtkPlaneWidgetCallback::SetMapper(vtkMapper* m)
{
	this->Mapper = m; 
}

void vtkPlaneWidgetCallback::SetPipelines(std::vector< mixed_pipeline >* p)
{
	this->pipelines = p; 
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

/* sts=8 ts=8 sw=80 tw=8 */

