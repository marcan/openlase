/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef OUTPUT_SETTINGS_H
#define OUTPUT_SETTINGS_H

#include "ui_output_settings.h"
#include "output.h"

#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QTransform>
#include <QFile>

class ControlPoint;

class OutputSetting : public QObject
{
	Q_OBJECT

public:
	OutputSetting(QWidget *widget, const QString &name, int& value, int bit = 0);
	int value;
	int bit;
	QString name;
	QWidget *widget;

	void updateValue();

signals:
	void valueChanged(int newValue);
	void valueChanging(int &newValue);

public slots:
	void setValue(int value);
	void setState(bool newValue);
	void setEnabled(bool enabled);

private:
	int &i_value;
	bool enabled;
	void updateTarget();
};

class OutputSettings : public QMainWindow, private Ui::OutputSettingsDLG
{
	Q_OBJECT
	
public:
	output_config_t cfg;
	OutputSettings(QWidget *parent = 0);
	~OutputSettings();

public slots:
	void on_aspectRatio_currentIndexChanged(int index);
	void on_aspectScale_toggled(bool state);
	void on_fitSquare_toggled(bool state);
	void on_outputTest_pressed();
	void on_outputTest_released();
	void on_resetTransform_clicked();

	void on_actionSaveSettings_triggered();
	void on_actionLoadSettings_triggered();

	void resizeEvent (QResizeEvent * event);
	void showEvent (QShowEvent * event);
	
	void pointMoved(ControlPoint *pt);

	void safeToggled(int &state);
	void updateSettings();

	void loadSettings(QString fileName);
	void saveSettings(QString fileName);

private:
	QTransform mtx;
	QGraphicsScene scene;
	QList<OutputSetting *> lsettings;
	ControlPoint *pt[4];
	QGraphicsPolygonItem pl;
	int currentAspect;
	void updateSettingsUI();
	void updatePoly();
	void updateMatrix();
	void resetDefaults();
	void resetPoints();
	void loadPoints();
	qreal getYRatio(int ratio);

	OutputSetting *findSetting(const QString &name);
	void addSetting(OutputSetting *setting);

};

class ControlPoint : public QGraphicsEllipseItem
{

public:
	OutputSettings *form;
	ControlPoint();
	
private:
	QVariant itemChange (GraphicsItemChange change, const QVariant & value);
};

#endif
