/* 
 * Copyright (C) 2011 Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
 * Authors: Ilaria Gori, Ugo Pattacini
 * email:  ilaria.gori@iit.it, ugo.pattacini@iit.it
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#include <stdio.h>
#include <stdarg.h>
#include <yarp/os/Time.h>
#include <yarp/math/Math.h>
#include <iCub/ctrl/math.h>
#include <iCub/pmp/pmp_server.h>
#include <iCub/pmp/private/pmp_helpers.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace iCub::pmp;


/************************************************************************/
Item::Item()
{
    name="";
    type="undefined";
    active=false;
    center.resize(3,0.0);
    orientation.resize(4,0.0);
    radius.resize(3,0.0);
    color.resize(3,0.0);
}


/************************************************************************/
bool Item::fromProperty(const Property &options)
{
    Property &opt=const_cast<Property&>(options);

    if (opt.check("name"))
        name=opt.find("name").asString().c_str();    

    if (opt.check("active"))
        active=opt.find("active").asString()=="on";    

    extractVector(opt,"center",center);
    extractVector(opt,"orientation",orientation);
    extractVector(opt,"radius",radius);
    extractVector(opt,"color",color);

    return true;
}


/************************************************************************/
Property Item::toProperty() const
{
    Vector &_center=const_cast<Vector&>(center);
    Vector &_orientation=const_cast<Vector&>(orientation);
    Vector &_radius=const_cast<Vector&>(radius);
    Vector &_color=const_cast<Vector&>(color);

    Value vcenter;      vcenter.fromString(("("+string(_center.toString().c_str())+")").c_str());
    Value vorientation; vorientation.fromString(("("+string(_orientation.toString().c_str())+")").c_str());
    Value vradius;      vradius.fromString(("("+string(_radius.toString().c_str())+")").c_str());
    Value vcolor;       vcolor.fromString(("("+string(_color.toString().c_str())+")").c_str());

    Property prop;
    prop.put("name",name.c_str());
    prop.put("type",type.c_str());
    prop.put("active",active?"on":"off");
    prop.put("center",vcenter);
    prop.put("orientation",vorientation);
    prop.put("radius",vradius);
    prop.put("color",vcolor);

    return prop;
}


/************************************************************************/
Target_MSD::Target_MSD() : K(0.0), D(0.0)
{
    type="target_msd";
    color[0]=0.0;
    color[1]=255.0;
    color[2]=0.0;
}


/************************************************************************/
bool Target_MSD::fromProperty(const Property &options)
{
    // call the father's method
    Item::fromProperty(options);

    Property &opt=const_cast<Property&>(options);

    if (opt.check("K"))
        K=opt.find("K").asDouble();

    if (opt.check("D"))
        D=opt.find("D").asDouble();

    return true;
}


/************************************************************************/
Property Target_MSD::toProperty() const
{
    // call the father's method
    Property prop=Item::toProperty();

    prop.put("K",K);
    prop.put("D",D);

    return prop;
}


/************************************************************************/
Vector Target_MSD::getField(const Vector &x, const Vector &xdot)
{
    Vector F;

    if (active)
    {
        F=K*(center-getVectorPos(x))-D*getVectorPos(xdot);
        F.push_back(0.0);
        F.push_back(0.0);
        F.push_back(0.0);
        F.push_back(0.0);
    }
    else
        F.resize(x.length(),0.0);

    return F;
}


/************************************************************************/
Obstacle_Gaussian::Obstacle_Gaussian() : G(0.0), cut_tails(false)
{
    type="obstacle_gaussian";
    color[0]=255.0;
    color[1]=0.0;
    color[2]=0.0;
}


/************************************************************************/
bool Obstacle_Gaussian::fromProperty(const Property &options)
{
    // call the father's method
    Item::fromProperty(options);

    Property &opt=const_cast<Property&>(options);

    if (opt.check("G"))
        G=opt.find("G").asDouble();

    if (opt.check("cut_tails"))
        cut_tails=opt.find("cut_tails").asString()=="on";

    return true;
}


/************************************************************************/
Property Obstacle_Gaussian::toProperty() const
{
    // call the father's method
    Property prop=Item::toProperty();

    prop.put("G",G);
    prop.put("cut_tails",cut_tails?"on":"off");

    return prop;
}


/************************************************************************/
Vector Obstacle_Gaussian::getField(const Vector &x, const Vector &xdot)
{
    Vector zeros(x.length()); zeros=0.0;
    if (!active)
        return zeros;

    Matrix Hro=axis2dcm(orientation);
    Hro(0,3)=center[0];
    Hro(1,3)=center[1];
    Hro(2,3)=center[2];

    Vector xr=getVectorPos(x);
    xr.push_back(1.0);
    
    Vector xo=SE3inv(Hro)*xr;

    xr.pop_back();
    xo.pop_back();

    Vector ell=(xo*xo)/(radius*radius);
    double dist=ell[0]+ell[1]+ell[2];

    if (cut_tails && (dist>1.0))
        return zeros;

    Vector dx=xr-center;
    Vector F=(G*exp(-dist)/norm(dx))*dx;
    F.push_back(0.0);
    F.push_back(0.0);
    F.push_back(0.0);
    F.push_back(0.0);

    return F;
}


/************************************************************************/
GuiReporter::GuiReporter()
{
    server=NULL;
}


/************************************************************************/
void GuiReporter::setServer(PmpServer *server)
{
    this->server=server;
}


/************************************************************************/
void GuiReporter::report(const PortInfo &info)
{
    if ((server!=NULL) && info.created && !info.incoming)
        server->scheduleInitGuiTrajectory();
}


/************************************************************************/
PmpServer::PmpServer() : RateThread(20), If(0.0,Vector(7)), Iv(0.0,Vector(7))
{
    isOpen=false;
    fieldEnabled=false;
    controlEnabled=false;
    simulationEnabled=false;
    simulationFirstStep=false;
    doInitGuiTrajectory=false;
    offlineMode=false;
    verbosity=0;
    name="";
    activeIF="";
    iCtrlRight=NULL;
    iCtrlLeft=NULL;
    itCnt=0;
    doTrajectoryCnt=0;

    xdot.resize(7,0.0);
    x.resize(7,0.0);
    xhat.resize(7,0.0);

    toolFrame=invToolFrame=eye(4,4);
}


/************************************************************************/
PmpServer::~PmpServer()
{
    close();
}


/************************************************************************/
int PmpServer::printMessage(const int level, const char *format, ...) const
{
    if (verbosity>=level)
    {
        fprintf(stdout,"*** %s: ",name.c_str());
    
        va_list ap;
        va_start(ap,format);
        int ret=vfprintf(stdout,format,ap);
        va_end(ap);
        
        return ret;
    }
    else
        return -1;
}


/************************************************************************/
bool PmpServer::open(const Property &options)
{
    Property &opt=const_cast<Property&>(options);

    verbosity=opt.check("verbosity",Value(0)).asInt();
    device=opt.check("device",Value("cartesiancontrollerclient")).asString().c_str();
    name=opt.check("name",Value("pmp_server")).asString().c_str();
    robot=opt.check("robot",Value("icub")).asString().c_str();
    part=opt.check("part",Value("both_arms")).asString().c_str();
    offlineMode=opt.check("offline");

    if ((part!="right_arm") && (part!="left_arm") && (part!="both_arms"))
    {
        printMessage(1,"server failed to open, unknown part specified\n");
        return false;
    }

    period=opt.check("period",Value(20)).asInt();

    double Ts=(double)period/1000.0;

    If.setTs(Ts);
    Iv.setTs(Ts);

    If.reset(xdot);
    Iv.reset(x);

    reporter.setServer(this);
    gui.setReporter(reporter);

    data.open(("/"+name+"/data:o").c_str());    
    gui.open(("/"+name+"/gui:o").c_str());
    rpc.open(("/"+name+"/rpc").c_str());
    rpc.setReader(*this);

    // request high resolution scheduling straightaway
    Time::turboBoost();
    
    setRate(period);
    start();

    if (!offlineMode)
    {
        if ((part=="right_arm") || (part=="both_arms"))
        {
            Property optCtrlRight;
            optCtrlRight.put("device",device.c_str());
            optCtrlRight.put("remote",("/"+robot+"/cartesianController/right_arm").c_str());
            optCtrlRight.put("local",("/"+name+"_right/cartesian").c_str());

            if (dCtrlRight.open(optCtrlRight))
                dCtrlRight.view(iCtrlRight);
            else
                return false;
        }
        if ((part=="left_arm") || (part=="both_arms"))
        {
            Property optCtrlLeft;
            optCtrlLeft.put("device",device.c_str());
            optCtrlLeft.put("remote",("/"+robot+"/cartesianController/left_arm").c_str());
            optCtrlLeft.put("local",("/"+name+"_left/cartesian").c_str());

            if (dCtrlLeft.open(optCtrlLeft))
                dCtrlLeft.view(iCtrlLeft);
            else
                return false;

        }

        if ((part=="both_arms") || (part=="right_arm"))
        {
            iCtrlActive=iCtrlRight;
            activeIF="right";
        }
        else
        {
            iCtrlActive=iCtrlLeft;
            activeIF="left";
        }

        Vector dof;
        iCtrlActive->getDOF(dof);
        qhat.resize(dof.length(),0.0);

        t0=Time::now();
    }

    printMessage(1,"server successfully open%s\n", offlineMode?" (offline mode)":"");

    return isOpen=true;
}


/************************************************************************/
void PmpServer::close()
{
    if (isOpen)
    {
        if (isRunning())
            stop();
        if (!offlineMode)
        {
            if ((part=="right_arm") || (part=="both_arms"))
                dCtrlRight.close();
            if ((part=="left_arm") || (part=="both_arms"))
                dCtrlLeft.close();
        }

        mutex.wait();

        guiQueue.clear();

        for (map<int,Item*>::iterator it=table.begin(); it!=table.end(); it++)
        {            
            eraseGuiItem(GuiRequest("erase",it));

            delete it->second;

            printMessage(1,"erasing item %d\n",it->first);

            Time::delay(0.02);
        }

        table.clear();

        eraseGuiTrajectory();

        data.interrupt();
        data.close();

        gui.interrupt();
        gui.close();

        isOpen=false;

        mutex.post();

        rpc.interrupt();
        rpc.close();

        printMessage(1,"server closed\n");
    }
    else
        printMessage(3,"server is already closed\n");
}


/************************************************************************/
Item* PmpServer::itemFactory(const Property &options)
{
    Property &opt=const_cast<Property&>(options);
    Item *item=NULL;    

    if (opt.check("type"))
    {
        string name=opt.check("name",Value("")).asString().c_str();
        string type=opt.find("type").asString().c_str();

        printMessage(2,"creating \"%s\" of type %s ...\n",name.c_str(),type.c_str());

        if (type=="target_msd")
            item=new Target_MSD;
        else if (type=="obstacle_gaussian")
            item=new Obstacle_Gaussian;
    }
    else
        printMessage(2,"option \"type\" not found\n");

    printMessage(2,"%s\n",item==NULL?"unknown type":"successfully created");

    return item;
}


/************************************************************************/
bool PmpServer::addItem(const Property &options, int &item)
{
    if (isOpen)
    {
        printMessage(2,"received request for adding item: %s\n",
                     options.toString().c_str());

        if (Item *pItem=itemFactory(options))
        {
            // configure item
            pItem->fromProperty(options);
            table[item=itCnt++]=pItem;
            map<int,Item*>::iterator it=table.find(item);
            pushUpdateGuiItem(it);

            printMessage(1,"added item %s\n",pItem->toProperty().toString().c_str());
            return true;
        }
        else
        {
            printMessage(1,"wrong request detected!\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::eraseItem(const int item)
{
    if (isOpen)
    {
        printMessage(2,"received request for erasing item %d\n",item);

        map<int,Item*>::iterator it=table.find(item);
        if (it!=table.end())
        {
            pushEraseGuiItem(it);

            delete it->second;
            table.erase(it);

            printMessage(1,"item %d scheduled for erasing\n",item);
            return true;
        }
        else
        {
            printMessage(1,"item %d not found!\n",item);
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::clearItems()
{
    if (isOpen)
    {
        for (map<int,Item*>::iterator it=table.begin(); it!=table.end(); it++)
        {
            pushEraseGuiItem(it);
            delete it->second;
        }

        table.clear();
        printMessage(1,"all items have been scheduled for erasing\n");
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getItems(Bottle &items) const
{
    if (isOpen)
    {
        items.clear();
        for (map<int,Item*>::const_iterator it=table.begin(); it!=table.end(); it++)
            items.addInt(it->first);

        printMessage(1,"list of items ids prepared for sending: (%s)\n",
                     items.toString().c_str());
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::setProperty(const int item, const Property &options)
{
    if (isOpen)
    {
        printMessage(2,"received request for setting item %d property: %s\n",
                     item,options.toString().c_str());

        map<int,Item*>::iterator it=table.find(item);
        if (it!=table.end())
        {
            it->second->fromProperty(options);
            pushUpdateGuiItem(it);

            printMessage(1,"item %d property successfully updated: %s\n",
                         item,it->second->toProperty().toString().c_str());
            return true;
        }
        else
        {
            printMessage(1,"item %d not found!\n",item);
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getProperty(const int item, Property &options) const
{
    if (isOpen)
    {
        printMessage(2,"received request for getting item %d property\n",item);

        map<int,Item*>::const_iterator it=table.find(item);
        if (it!=table.end())
        {
            options=it->second->toProperty();

            printMessage(1,"item %d property successfully retrieved: %s\n",
                         item,options.toString().c_str());
            return true;
        }
        else
        {
            printMessage(1,"item %d not found!\n",item);
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::enableField()
{
    if (isOpen)
    {
        fieldEnabled=true;

        printMessage(1,"field enabled\n");
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::disableField()
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            iCtrlActive->stopControl();
        }

        fieldEnabled=false;

        printMessage(1,"field disabled\n");
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getFieldStatus(bool &status) const
{
    if (isOpen)
    {
        status=fieldEnabled;

        printMessage(1,"field status = %s\n",status?"on":"off");
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::enableControl()
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            controlEnabled=true;
            printMessage(1,"control enabled\n");

            if (simulationEnabled)
            {
                simulationEnabled=false;
                printMessage(2,"simulation gets automatically disabled\n");
            }

            return true;
        }
        else
        {
            printMessage(1,"it is not possible to enable control in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::disableControl()
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            iCtrlActive->stopControl();

            controlEnabled=false;

            printMessage(1,"control disabled\n");
            return true;
        }
        else
        {
            printMessage(1,"it is not possible to disable control in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getControlStatus(bool &status) const
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            status=controlEnabled;

            printMessage(1,"control status = %s\n",status?"on":"off");
            return true;
        }
        else
        {
            printMessage(1,"there is no possibility to enable or disable control in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::enableSimulation()
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            simulationEnabled=true;
            simulationFirstStep=true;
            printMessage(1,"simulation enabled\n");

            if (controlEnabled)
            {
                controlEnabled=false;
                printMessage(2,"control gets automatically disabled\n");
            }

            return true;
        }
        else
        {
            printMessage(1,"it is not possible to enable simulation in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::disableSimulation()
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            simulationEnabled=false;

            printMessage(1,"simulation disabled\n");
            return true;
        }
        else
        {
            printMessage(1,"it is not possible to disable simulation in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getSimulationStatus(bool &status) const
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            status=simulationEnabled;

            printMessage(1,"simulation status = %s\n",status?"on":"off");
            return true;
        }
        else
        {
            printMessage(1,"there is no possibility to enable or disable simulation in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::setPeriod(const int period)
{
    if (isOpen)
    {
        this->period=period;
        double Ts=(double)this->period/1000.0;

        If.setTs(Ts);
        Iv.setTs(Ts);

        if (!offlineMode)
        {        
            setRate(this->period);
            printMessage(1,"thread period changed to %d [ms]\n",period);
        }
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getPeriod(int &period) const
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            period=(int)const_cast<PmpServer*>(this)->getRate();
            printMessage(1,"thread period is %d [ms]\n",period);
        }
        else
        {
            period=this->period;
            printMessage(1,"integration time is %d [ms]\n",this->period);
        }

        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::setPointState(const Vector &x, const Vector &o,
                              const Vector &xdot, const Vector &odot)
{
    if (isOpen)
    {
        copyVectorData(x,this->x);
        copyVectorData(o,this->x);
        copyVectorData(xdot,this->xdot);
        copyVectorData(odot,this->xdot);

        If.reset(this->xdot);
        Iv.reset(this->x);

        printMessage(1,"point state changed to x = %s; xdot = %s\n",
                     this->x.toString().c_str(),this->xdot.toString().c_str());
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::setPointStateToTool()
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            Vector x,o,xdot,odot;
            getTool(x,o);
            iCtrlActive->getTaskVelocities(xdot,odot);
            odot=0.0;

            copyVectorData(x,this->x);
            copyVectorData(o,this->x);
            copyVectorData(xdot,this->xdot);
            copyVectorData(odot,this->xdot);

            If.reset(this->xdot);
            Iv.reset(this->x);

            printMessage(1,"point state changed to x = %s; xdot = %s\n",
                         this->x.toString().c_str(),this->xdot.toString().c_str());
            return true;
        }
        else
        {
            printMessage(1,"no connection with the robot in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::attachToolFrame(const yarp::sig::Vector &x, const yarp::sig::Vector &o)
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            if ((x.length()<3) || (o.length()<4))
            {
                printMessage(1,"problem with vector lengths\n");
                return false;
            }
            else
            {
                toolFrame=axis2dcm(o);
                toolFrame(0,3)=x[0];
                toolFrame(1,3)=x[1];
                toolFrame(2,3)=x[2];

                invToolFrame=SE3inv(toolFrame);

                printMessage(1,"attach tool frame = %s\n",toolFrame.toString().c_str());
                return true;
            }
        }
        else
        {
            printMessage(1,"it is not possible to attach frame in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getToolFrame(yarp::sig::Vector &x, yarp::sig::Vector &o) const
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            x.resize(3);
            x[0]=toolFrame(0,3);
            x[1]=toolFrame(1,3);
            x[2]=toolFrame(2,3);
            o=dcm2axis(toolFrame);

            printMessage(1,"tool frame currently attached is = %s\n",toolFrame.toString().c_str());
            return true;
        }
        else
        {
            printMessage(1,"there is no tool in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::removeToolFrame()
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            toolFrame=invToolFrame=eye(4,4);

            printMessage(1,"tool frame removed\n");
            return true;
        }
        else
        {
            printMessage(1,"there is no tool in offline mode\n");
            return false;
        }   
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getTool(yarp::sig::Vector &x, yarp::sig::Vector &o) const
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            Vector pos,orien;
            iCtrlActive->getPose(pos,orien);

            Matrix frame1=axis2dcm(orien);
            frame1(0,3)=pos[0];
            frame1(1,3)=pos[1];
            frame1(2,3)=pos[2];

            Matrix frame2=frame1*toolFrame;
            x.resize(3);
            x[0]=frame2(0,3);
            x[1]=frame2(1,3);
            x[2]=frame2(2,3);
            o=dcm2axis(frame2);

            printMessage(1,"tool state is = %s\n",frame2.toString().c_str());
            return true;
        }
        else
        {
            printMessage(1,"there is no tool in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getPointState(Vector &x, Vector &o, Vector &xdot, Vector &odot) const
{
    if (isOpen)
    {
        x=getVectorPos(this->x);
        o=getVectorOrien(this->x);
        xdot=getVectorPos(this->xdot);
        odot=getVectorOrien(this->xdot);

        Vector &_x=const_cast<Vector&>(this->x);
        Vector &_xdot=const_cast<Vector&>(this->xdot);

        printMessage(1,"point state is x = %s; xdot = %s\n",
                     _x.toString().c_str(),_xdot.toString().c_str());
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getField(Vector &field) const
{
    if (isOpen)
    {
        field.resize(x.length(),0.0);
        for (map<int,Item*>::const_iterator it=table.begin(); it!=table.end(); it++) 
            field=field+it->second->getField(x,xdot);

        printMessage(1,"field = %s\n",field.toString().c_str());
        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::getSimulation(Vector &xhat, Vector &ohat, Vector &qhat) const
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            xhat=getVectorPos(xhat);
            ohat=getVectorOrien(xhat);
            qhat=this->qhat;

            Vector &_xhat=const_cast<Vector&>(this->xhat);

            printMessage(1,"simulated end-effector pose is xhat = %s; part configuration is qhat = %s\n",
                         _xhat.toString().c_str(),qhat.toString().c_str());
            return true;
        }
        else
        {
            printMessage(1,"there is no possibility to enable simulation in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::read(ConnectionReader &connection)
{
    Bottle cmd, reply;
    cmd.read(connection);

    mutex.wait();

    if (cmd.size()==0)
        reply.addVocab(PMP_VOCAB_CMD_NACK);
    else switch(cmd.get(0).asVocab())
    {
        //-----------------
         case PMP_VOCAB_CMD_PING:
         {
             reply.addVocab(PMP_VOCAB_CMD_ACK);
             break;
         }         

         //-----------------
         case PMP_VOCAB_CMD_ADD:
         {
             if (cmd.size()<2)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else
             {
                 int item;
                 if (addItem(extractProperty(cmd.get(1)),item))
                 {
                     reply.addVocab(PMP_VOCAB_CMD_ACK);
                     reply.addInt(item);
                 }
                 else
                     reply.addVocab(PMP_VOCAB_CMD_NACK);
             }

             break;
         }         

         //-----------------
         case PMP_VOCAB_CMD_DEL:
         {
             if (cmd.size()<2)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else
             {
                 if (eraseItem(cmd.get(1).asInt()))
                     reply.addVocab(PMP_VOCAB_CMD_ACK);
                 else
                     reply.addVocab(PMP_VOCAB_CMD_NACK);
             }

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_SET:
         {
             if (cmd.size()<3)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else if (setProperty(cmd.get(1).asInt(),extractProperty(cmd.get(2))))
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_GET:
         {
             if (cmd.size()<2)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else
             {
                 Property options;
                 if (getProperty(cmd.get(1).asInt(),options))
                 {
                     Value val;
                     val.fromString(("("+string(options.toString().c_str())+")").c_str());

                     reply.addVocab(PMP_VOCAB_CMD_ACK);
                     reply.add(val);
                 }
                 else
                     reply.addVocab(PMP_VOCAB_CMD_NACK);
             }

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_LIST:
         {
             Bottle items;
             if (getItems(items))
             {
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.addList().append(items);
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_CLEAR:
         {
             if (clearItems())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_ENFIELD:
         {
             if (enableField())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_DISFIELD:
         {
             if (disableField())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_STATFIELD:
         {
             bool status;
             if (getFieldStatus(status))
             {
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.addString(status?"on":"off");
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_ENCTRL:
         {
             if (enableControl())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_DISCTRL:
         {
             if (disableControl())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_STATCTRL:
         {
             bool status;
             if (getControlStatus(status))
             {
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.addString(status?"on":"off");
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_ENSIM:
         {
             if (enableSimulation())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_DISSIM:
         {
             if (disableSimulation())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_STATSIM:
         {
             bool status;
             if (getSimulationStatus(status))
             {
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.addString(status?"on":"off");
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_SETPER:
         {
             if (cmd.size()<2)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else if (setPeriod(cmd.get(1).asInt()))
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }         

         //-----------------
         case PMP_VOCAB_CMD_GETPER:
         {
             int period;
             if (getPeriod(period))
             {
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.addInt(period);
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_SETACTIF:
         {
             if (cmd.size()<2)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else if (setActiveIF(cmd.get(1).asString().c_str()))
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_GETACTIF:
         {
             string _activeIF;
             if (getActiveIF(_activeIF))
             {
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.addString(_activeIF.c_str());
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_GETTRAJ:
         {
             Property options=extractProperty(cmd.get(1));
             if (options.isNull())
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else
             {
                 int maxIterations=options.find("maxIterations").asInt();
                 double Ts=options.find("Ts").asDouble();
                 deque<Vector> trajPos;
                 deque<Vector> trajOrien;
                 if (getTrajectory(trajPos,trajOrien,maxIterations,Ts))
                 {
                     reply.addVocab(PMP_VOCAB_CMD_ACK);

                     Bottle &bPos=reply.addList();
                     bPos.addString("trajPos");
                     for (unsigned int i=0; i<trajPos.size(); i++)
                     {
                         Bottle &point=bPos.addList();
                         for (int j=0; j<trajPos[i].size(); j++)
                            point.addDouble(trajPos[i][j]);
                     }

                     Bottle &bOrien=reply.addList();
                     bOrien.addString("trajOrien");
                     for (unsigned int i=0; i<trajOrien.size(); i++)
                     {
                         Bottle &point=bOrien.addList();
                         for (int j=0; j<trajOrien[i].size(); j++)
                            point.addDouble(trajOrien[i][j]);
                     }
                }
                else
                    reply.addVocab(PMP_VOCAB_CMD_NACK);
             }

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_SETSTATETOTOOL:
         {
             if (cmd.size()<1)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else
             {
                 if (setPointStateToTool())
                     reply.addVocab(PMP_VOCAB_CMD_ACK);
                 else
                     reply.addVocab(PMP_VOCAB_CMD_NACK);
             }

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_SETSTATE:
         {
             if (cmd.size()<2)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else
             {
                 Property options=extractProperty(cmd.get(1));
                 if (options.isNull())
                     reply.addVocab(PMP_VOCAB_CMD_NACK);
                 else
                 {
                     Vector pos,vel;
                     if (extractVector(options,"x",pos) && extractVector(options,"xdot",vel))
                     {
                         if (setPointState(getVectorPos(pos),getVectorOrien(pos),
                                           getVectorPos(vel),getVectorOrien(vel)))
                             reply.addVocab(PMP_VOCAB_CMD_ACK);
                         else
                             reply.addVocab(PMP_VOCAB_CMD_NACK);
                     }
                     else
                         reply.addVocab(PMP_VOCAB_CMD_NACK);
                 }
             }

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_GETSTATE:
         {
             if (isOpen)
             {
                 Property state=prepareData();
                 Value val_state;
                 val_state.fromString(("("+string(state.toString().c_str())+")").c_str());

                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.add(val_state);
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_ATTACHTOOLFRAME:
         {
             if (cmd.size()<2)
                 reply.addVocab(PMP_VOCAB_CMD_NACK);
             else
             {
                 Vector x,o;
                 Property options=extractProperty(cmd.get(1));
                 extractVector(options,"x",x);
                 extractVector(options,"o",o);

                 if (attachToolFrame(x,o))
                     reply.addVocab(PMP_VOCAB_CMD_ACK);
                 else
                     reply.addVocab(PMP_VOCAB_CMD_NACK);                 
             }

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_GETTOOLFRAME:
         {
             Vector x,o;
             if (getToolFrame(x,o))
             {
                 Value val_x; val_x.fromString(("("+string(x.toString().c_str())+")").c_str());
                 Value val_o; val_o.fromString(("("+string(o.toString().c_str())+")").c_str());

                 Property options;
                 options.put("x",val_x);
                 options.put("o",val_o);

                 Value val;
                 val.fromString(("("+string(options.toString().c_str())+")").c_str());

                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.add(val);
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);                 

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_REMOVETOOLFRAME:
         {
             if (removeToolFrame())
                 reply.addVocab(PMP_VOCAB_CMD_ACK);
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);                 

             break;
         }

         //-----------------
         case PMP_VOCAB_CMD_GETTOOL:
         {
             Vector x,o;
             if (getTool(x,o))
             {
                 Value val_x; val_x.fromString(("("+string(x.toString().c_str())+")").c_str());
                 Value val_o; val_o.fromString(("("+string(o.toString().c_str())+")").c_str());

                 Property options;
                 options.put("x",val_x);
                 options.put("o",val_o);

                 Value val;
                 val.fromString(("("+string(options.toString().c_str())+")").c_str());

                 reply.addVocab(PMP_VOCAB_CMD_ACK);
                 reply.add(val);
             }
             else
                 reply.addVocab(PMP_VOCAB_CMD_NACK);                 

             break;
         }

         //-----------------
         default:
             reply.addVocab(PMP_VOCAB_CMD_NACK);
    }

    mutex.post();

    ConnectionWriter *returnToSender=connection.getWriter();
    if (returnToSender!=NULL)
        reply.write(*returnToSender);    

    return true;
}


/************************************************************************/
Property PmpServer::prepareData()
{
    Vector field;
    getField(field);

    Value val_field; val_field.fromString(("("+string(field.toString().c_str())+")").c_str());
    Value val_xdot;  val_xdot.fromString(("("+string(xdot.toString().c_str())+")").c_str());  
    Value val_x;     val_x.fromString(("("+string(x.toString().c_str())+")").c_str());        
    Value val_xhat;  val_xhat.fromString(("("+string(xhat.toString().c_str())+")").c_str());  
    Value val_qhat;  val_qhat.fromString(("("+string(qhat.toString().c_str())+")").c_str());  

    Property out;
    out.put("field",val_field);
    out.put("xdot",val_xdot);
    out.put("x",val_x);
    out.put("xhat",val_xhat);
    out.put("qhat",val_qhat);

    return out;
}


/************************************************************************/
void PmpServer::getTargetForCartesianIF(Vector &pos, Vector &orien)
{
    pos=getVectorPos(x);
    orien=getVectorOrien(x);

    Matrix frame1=axis2dcm(orien);
    frame1(0,3)=pos[0];
    frame1(1,3)=pos[1];
    frame1(2,3)=pos[2];

    Matrix frame2=frame1*invToolFrame;
    pos[0]=frame2(0,3);
    pos[1]=frame2(1,3);
    pos[2]=frame2(2,3);
    orien=dcm2axis(frame2);
}


/************************************************************************/
bool PmpServer::getActiveIF(string &activeIF) const
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            activeIF=this->activeIF;
            printMessage(1,"active interface: %s\n",activeIF.c_str());
            return true;
        }
        else
        {
            activeIF="";
            printMessage(1,"no connection with the robot in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
bool PmpServer::setActiveIF(const string &activeIF)
{
    if (isOpen)
    {
        if (!offlineMode)
        {
            if (part=="both_arms")
            {
                if ((activeIF=="right") || (activeIF=="left"))
                {    
                    iCtrlActive->stopControl();
                    if (activeIF=="right")
                        iCtrlActive=iCtrlRight;
                    else
                        iCtrlActive=iCtrlLeft;
                    
                    this->activeIF=activeIF;
                    printMessage(1,"active interface successfully set to %s\n",activeIF.c_str());
                    return true;
                }
                else
                {
                    printMessage(1,"wrong value specified, it should be \"right\" or \"left\"\n");
                    return false;
                }
            }
            else
            {
                printMessage(1,"cannot swap arm\n");
                return false;
            }
        }
        else
        {            
            printMessage(1,"no connection with the robot in offline mode\n");
            return false;
        }
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
void PmpServer::scheduleInitGuiTrajectory()
{
    doInitGuiTrajectory=true;
}


/************************************************************************/
void PmpServer::initGuiTrajectory()
{
    if (gui.getOutputCount()>0)
    {
        Bottle obj;
        obj.addString("trajectory");
        obj.addString(part.c_str());    // trajectory identifier
        obj.addString(part.c_str());    // trajectory name
        obj.addInt(512);                // max samples in circular queue
        obj.addDouble(5.0);             // lifetime of samples
        obj.addInt(0);                  // col R
        obj.addInt(0);                  // col G
        obj.addInt(255);                // col B
        obj.addDouble(0.5);             // alpha [0,1]
        obj.addDouble(5.0);             // line width

        gui.write(obj);
        printMessage(4,"initializing gui trajectory point\n");
    }
}


/************************************************************************/
void PmpServer::updateGuiTrajectory()
{
    if (gui.getOutputCount()>0)
    {
        Bottle obj;
        obj.addString("addpoint");
        obj.addString(part.c_str());    // trajectory identifier
        obj.addDouble(1000.0*x[0]);     // posX [mm]
        obj.addDouble(1000.0*x[1]);     // posY [mm]
        obj.addDouble(1000.0*x[2]);     // posZ [mm]

        gui.write(obj);
        printMessage(4,"updating gui trajectory point\n");
    }
}


/************************************************************************/
void PmpServer::eraseGuiTrajectory()
{
    if (gui.getOutputCount()>0)
    {
        Bottle obj;
        obj.addString("delete");
        obj.addString(part.c_str());

        gui.write(obj);
        printMessage(4,"erasing gui tracjectory point\n");
    }
}


/************************************************************************/
void PmpServer::updateGuiItem(const GuiRequest &req)
{    
    if (gui.getOutputCount()>0)
    {
        map<int,Item*>::iterator it=req.getIter();
        string name=req.getName();
        int item=it->first;
        Item *pItem=it->second;

        Vector ypr=CTRL_RAD2DEG*dcm2euler(axis2dcm(pItem->orientation));

        Bottle obj;
        obj.addString("object");        
        obj.addString(name.c_str());
        obj.addDouble(1000.0*pItem->radius[0]);     // dimX [mm]
        obj.addDouble(1000.0*pItem->radius[1]);     // dimY [mm]
        obj.addDouble(1000.0*pItem->radius[2]);     // dimZ [mm]
        obj.addDouble(1000.0*pItem->center[0]);     // posX [mm]
        obj.addDouble(1000.0*pItem->center[1]);     // posY [mm]
        obj.addDouble(1000.0*pItem->center[2]);     // posZ [mm]
        obj.addDouble(ypr[2]);                      // rotX [deg]
        obj.addDouble(ypr[1]);                      // rotY [deg]
        obj.addDouble(ypr[0]);                      // rotZ [deg]
        obj.addInt((int)pItem->color[0]);           // col R
        obj.addInt((int)pItem->color[1]);           // col G
        obj.addInt((int)pItem->color[2]);           // col B
        obj.addDouble(pItem->active?1.0:0.25);      // alpha [0,1]

        gui.write(obj);
        printMessage(4,"updating gui item %s\n",name.c_str());
    }
}


/************************************************************************/
void PmpServer::eraseGuiItem(const GuiRequest &req)
{
    if (gui.getOutputCount()>0)
    {
        Bottle obj;
        obj.addString("delete");
        obj.addString(req.getName().c_str());

        gui.write(obj);
        printMessage(1,"erasing gui item %s\n",name.c_str());
    }
}


/************************************************************************/
void PmpServer::pushUpdateGuiItem(map<int,Item*>::iterator &it)
{
    GuiRequest update("update",it);

    for (unsigned int i=0; i<guiQueue.size(); i++)
    {
        if (guiQueue[i]==update)
        {
            printMessage(5,"gui item %d update is already scheduled\n",it->first);
            return;
        }
    }

    guiQueue.push_back(update);
    printMessage(5,"scheduled request for gui item %d update\n",it->first);
}


/************************************************************************/
void PmpServer::pushEraseGuiItem(map<int,Item*>::iterator &it)
{
    int item=it->first;
    GuiRequest update("update",it);

    for (unsigned int i=0; i<guiQueue.size(); i++)
    {
        if (guiQueue[i]==update)
        {
            guiQueue.erase(guiQueue.begin()+i);
            printMessage(5,"remove schedule for gui item %d update\n",item);
            break;
        }
    }

    guiQueue.push_back(GuiRequest("erase",it));
    printMessage(5,"scheduled request for gui item %d erase\n",item);
}


/************************************************************************/
void PmpServer::handleGuiQueue()
{
    if (doInitGuiTrajectory)
    {
        initGuiTrajectory();
        doInitGuiTrajectory=false;
    }
    // doTrajectoryCnt serves to avoid updating continuosly
    // the items and delaying the trajectory too much
    else if (guiQueue.size() && (doTrajectoryCnt<3))
    {
        GuiRequest guiReq=guiQueue.front();
        guiQueue.pop_front();

        if (guiReq.getType()=="update")
            updateGuiItem(guiReq);
        else if (guiReq.getType()=="erase")
            eraseGuiItem(guiReq);
        else
            printMessage(1,"unknown gui request received!\n");

        doTrajectoryCnt++;
    }
    else
    {
        updateGuiTrajectory();
        doTrajectoryCnt=0;
    }
}


/************************************************************************/
bool PmpServer::getTrajectory(deque<Vector> &trajPos, deque<Vector> &trajOrien,
                              const unsigned int maxIterations, const double Ts)
{
    if (isOpen)
    {
        Vector xdotOffline=xdot;
        Vector xOffline=x;

        double _Ts=(Ts<=PMP_DEFAULT_TS_DISABLED)?(double)period/1000.0:Ts;
        Integrator IfOffline(_Ts,xdotOffline);
        Integrator IvOffline(_Ts,xOffline);

        unsigned int iteration=0;
        while (iteration++<maxIterations)
        {
            Vector field;
            getField(field);

            xdotOffline=IfOffline.integrate(field);
            xOffline=IvOffline.integrate(xdotOffline);

            trajPos.push_back(getVectorPos(xOffline));
            trajOrien.push_back(getVectorOrien(xOffline));
        }       

        return true;
    }
    else
    {
        printMessage(1,"server is not open\n");
        return false;
    }
}


/************************************************************************/
void PmpServer::run()
{
    mutex.wait();

    if (!offlineMode)
    {
        if (fieldEnabled)
        {
            printMessage(4,"processing %d items\n",table.size());
            
            Vector field;
            getField(field);

            xdot=If.integrate(field);
            x=Iv.integrate(xdot);

            if (controlEnabled)
            {
                Vector pos,orien;
                getTargetForCartesianIF(pos,orien);
                iCtrlActive->goToPose(pos,orien);
            }

            if (simulationEnabled)
            {            
                Vector pos,orien,xdhat,odhat;
                getTargetForCartesianIF(pos,orien);

                if (simulationFirstStep)
                {                
                    iCtrlActive->askForPose(pos,orien,xdhat,odhat,qhat);
                    simulationFirstStep=false;
                }
                else
                {
                    Vector dof;
                    iCtrlActive->getDOF(dof);

                    Vector q0;
                    for (int i=0; i<dof.length(); i++)
                        if (dof[i]>0.0)
                            q0.push_back(qhat[i]);

                    iCtrlActive->askForPose(q0,pos,orien,xdhat,odhat,qhat);
                }

                copyVectorData(xdhat,xhat);
                copyVectorData(odhat,xhat);
            }
        }
    }

    if (data.getOutputCount()>0)
    {
        Property out=prepareData();
        data.write(out);
    }    

    // avoid running faster than what is necessary
    if (Time::now()-t0>0.03)
    {
        handleGuiQueue();
        t0=Time::now();
    }

    mutex.post();
}


    

