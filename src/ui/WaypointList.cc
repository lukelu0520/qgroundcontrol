/*=====================================================================

PIXHAWK Micro Air Vehicle Flying Robotics Toolkit

(c) 2009, 2010 PIXHAWK PROJECT  <http://pixhawk.ethz.ch>

This file is part of the PIXHAWK project

    PIXHAWK is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PIXHAWK is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PIXHAWK. If not, see <http://www.gnu.org/licenses/>.

======================================================================*/

/**
 * @file
 *   @brief Waypoint list widget
 *
 *   @author Lorenz Meier <mavteam@student.ethz.ch>
 *   @author Benjamin Knecht <mavteam@student.ethz.ch>
 *   @author Petri Tanskanen <mavteam@student.ethz.ch>
 *
 */

#include "WaypointList.h"
#include "ui_WaypointList.h"
#include <UASInterface.h>
#include <UASManager.h>
#include <QDebug>
#include <QFileDialog>

WaypointList::WaypointList(QWidget *parent, UASInterface* uas) :
        QWidget(parent),
        uas(NULL),
        m_ui(new Ui::WaypointList)
{
    m_ui->setupUi(this);

    listLayout = new QVBoxLayout(m_ui->listWidget);
    listLayout->setSpacing(6);
    listLayout->setMargin(0);
    listLayout->setAlignment(Qt::AlignTop);
    m_ui->listWidget->setLayout(listLayout);

    wpViews = QMap<Waypoint*, WaypointView*>();

    this->uas = NULL;

    // ADD WAYPOINT
    // Connect add action, set right button icon and connect action to this class
    connect(m_ui->addButton, SIGNAL(clicked()), m_ui->actionAddWaypoint, SIGNAL(triggered()));
    connect(m_ui->actionAddWaypoint, SIGNAL(triggered()), this, SLOT(add()));

    // SEND WAYPOINTS
    connect(m_ui->transmitButton, SIGNAL(clicked()), this, SLOT(transmit()));

    // REQUEST WAYPOINTS
    connect(m_ui->readButton, SIGNAL(clicked()), this, SLOT(read()));

    // SAVE/LOAD WAYPOINTS
    connect(m_ui->saveButton, SIGNAL(clicked()), this, SLOT(saveWaypoints()));
    connect(m_ui->loadButton, SIGNAL(clicked()), this, SLOT(loadWaypoints()));

    connect(UASManager::instance(), SIGNAL(activeUASSet(UASInterface*)), this, SLOT(setUAS(UASInterface*)));

    // STATUS LABEL
    updateStatusLabel("");

    // SET UAS AFTER ALL SIGNALS/SLOTS ARE CONNECTED
    setUAS(uas);
}

WaypointList::~WaypointList()
{
    delete m_ui;
}

void WaypointList::updateStatusLabel(const QString &string)
{
    m_ui->statusLabel->setText(string);
}

void WaypointList::setUAS(UASInterface* uas)
{
    if (this->uas == NULL && uas != NULL)
    {
        this->uas = uas;

        connect(&uas->getWaypointManager(), SIGNAL(updateStatusString(const QString &)),                                this, SLOT(updateStatusLabel(const QString &)));
        connect(&uas->getWaypointManager(), SIGNAL(waypointUpdated(int,quint16,double,double,double,double,bool,bool,double,int)), this, SLOT(setWaypoint(int,quint16,double,double,double,double,bool,bool,double,int)));
        connect(&uas->getWaypointManager(), SIGNAL(currentWaypointChanged(quint16)),                                    this, SLOT(currentWaypointChanged(quint16)));

        connect(this, SIGNAL(sendWaypoints(const QVector<Waypoint*> &)),    &uas->getWaypointManager(), SLOT(sendWaypoints(const QVector<Waypoint*> &)));
        connect(this, SIGNAL(requestWaypoints()),                           &uas->getWaypointManager(), SLOT(requestWaypoints()));
        connect(this, SIGNAL(clearWaypointList()),                          &uas->getWaypointManager(), SLOT(clearWaypointList()));
    }
}

void WaypointList::setWaypoint(int uasId, quint16 id, double x, double y, double z, double yaw, bool autocontinue, bool current, double orbit, int holdTime)
{
    if (uasId == this->uas->getUASID())
    {
        Waypoint* wp = new Waypoint(id, x, y, z, yaw, autocontinue, current, orbit, holdTime);
        addWaypoint(wp);
    }
}

void WaypointList::waypointReached(UASInterface* uas, quint16 waypointId)
{
    Q_UNUSED(uas);
    qDebug() << "Waypoint reached: " << waypointId;

    updateStatusLabel(QString("Waypoint %1 reached.").arg(waypointId));
}

void WaypointList::currentWaypointChanged(quint16 seq)
{
    if (seq < waypoints.size())
    {
        for(int i = 0; i < waypoints.size(); i++)
        {
            WaypointView* widget = wpViews.find(waypoints[i]).value();

            if (waypoints[i]->getId() == seq)
            {
                waypoints[i]->setCurrent(true);
                widget->setCurrent(true);
            }
            else
            {
                waypoints[i]->setCurrent(false);
                widget->setCurrent(false);
            }
        }
        redrawList();
    }
}

void WaypointList::read()
{
    while(waypoints.size()>0)
    {
        removeWaypoint(waypoints[0]);
    }

    emit requestWaypoints();
}

void WaypointList::transmit()
{
    emit sendWaypoints(waypoints);
    //emit requestWaypoints(); FIXME
}

void WaypointList::add()
{
    // Only add waypoints if UAS is present
    if (uas)
    {
        if (waypoints.size() > 0)
        {
            Waypoint *last = waypoints.at(waypoints.size()-1);
            addWaypoint(new Waypoint(waypoints.size(), last->getX(), last->getY(), last->getZ(), last->getYaw(), last->getAutoContinue(), false, last->getOrbit(), last->getHoldTime()));
        }
        else
        {
            addWaypoint(new Waypoint(waypoints.size(), 1.1, 1.1, -0.8, 0.0, true, true, 0.1, 1000));
        }
    }
}

void WaypointList::addWaypoint(Waypoint* wp)
{
    waypoints.push_back(wp);
    if (!wpViews.contains(wp))
    {
        WaypointView* wpview = new WaypointView(wp, this);
        wpViews.insert(wp, wpview);
        listLayout->addWidget(wpViews.value(wp));
        connect(wpview, SIGNAL(moveDownWaypoint(Waypoint*)), this, SLOT(moveDown(Waypoint*)));
        connect(wpview, SIGNAL(moveUpWaypoint(Waypoint*)), this, SLOT(moveUp(Waypoint*)));
        connect(wpview, SIGNAL(removeWaypoint(Waypoint*)), this, SLOT(removeWaypoint(Waypoint*)));
        connect(wpview, SIGNAL(currentWaypointChanged(quint16)), this, SLOT(currentWaypointChanged(quint16)));
    }
}

void WaypointList::redrawList()
{
    // Clear list layout
    if (!wpViews.empty())
    {
        QMapIterator<Waypoint*,WaypointView*> viewIt(wpViews);
        viewIt.toFront();
        while(viewIt.hasNext())
        {
            viewIt.next();
            listLayout->removeWidget(viewIt.value());
        }
        // Re-add waypoints
        for(int i = 0; i < waypoints.size(); i++)
        {
            listLayout->addWidget(wpViews.value(waypoints[i]));
        }
    }
}

void WaypointList::moveUp(Waypoint* wp)
{
    int id = wp->getId();
    if (waypoints.size() > 1 && waypoints.size() > id)
    {
        Waypoint* temp = waypoints[id];
        if (id > 0)
        {
            waypoints[id] = waypoints[id-1];
            waypoints[id-1] = temp;
            waypoints[id-1]->setId(id-1);
            waypoints[id]->setId(id);
        }
        else
        {
            waypoints[id] = waypoints[waypoints.size()-1];
            waypoints[waypoints.size()-1] = temp;
            waypoints[waypoints.size()-1]->setId(waypoints.size()-1);
            waypoints[id]->setId(id);
        }
        redrawList();
    }
}

void WaypointList::moveDown(Waypoint* wp)
{
    int id = wp->getId();
    if (waypoints.size() > 1 && waypoints.size() > id)
    {
        Waypoint* temp = waypoints[id];
        if (id != waypoints.size()-1)
        {
            waypoints[id] = waypoints[id+1];
            waypoints[id+1] = temp;
            waypoints[id+1]->setId(id+1);
            waypoints[id]->setId(id);
        }
        else
        {
            waypoints[id] = waypoints[0];
            waypoints[0] = temp;
            waypoints[0]->setId(0);
            waypoints[id]->setId(id);
        }
        redrawList();
    }
}

void WaypointList::removeWaypoint(Waypoint* wp)
{
    // Delete from list
    if (wp != NULL)
    {
        waypoints.remove(wp->getId());
        for(int i = wp->getId(); i < waypoints.size(); i++)
        {
            waypoints[i]->setId(i);
        }

        // Remove from view
        WaypointView* widget = wpViews.find(wp).value();
        wpViews.remove(wp);
        widget->hide();
        listLayout->removeWidget(widget);
        delete wp;
    }
}

void WaypointList::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::LanguageChange:
        m_ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void WaypointList::saveWaypoints()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), "./waypoints.txt", tr("Waypoint File (*.txt)"));
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    for (int i = 0; i < waypoints.size(); i++)
    {
        Waypoint* wp = waypoints[i];
        in << "\t" << wp->getId() << "\t" << wp->getX() << "\t" << wp->getY()  << "\t" << wp->getZ()  << "\t" << wp->getYaw()  << "\t" << wp->getAutoContinue() << "\t" << wp->getCurrent() << "\t" << wp->getOrbit() << "\t" << wp->getHoldTime() << "\n";
        in.flush();
    }
    file.close();
}

void WaypointList::loadWaypoints()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load File"), ".", tr("Waypoint File (*.txt)"));
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while(waypoints.size()>0)
    {
        removeWaypoint(waypoints[0]);
    }

    QTextStream in(&file);
    while (!in.atEnd())
    {
        QStringList wpParams = in.readLine().split("\t");
        if (wpParams.size() == 10)
            addWaypoint(new Waypoint(wpParams[1].toInt(), wpParams[2].toDouble(), wpParams[3].toDouble(), wpParams[4].toDouble(), wpParams[5].toDouble(), (wpParams[6].toInt() == 1 ? true : false), (wpParams[7].toInt() == 1 ? true : false), wpParams[8].toDouble(), wpParams[9].toInt()));
    }
    file.close();
}

