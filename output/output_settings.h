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

class ControlPoint;

class OutputSettings : public QMainWindow, private Ui::OutputSettingsDLG
{
	Q_OBJECT
	
public:
	output_config_t cfg;
	OutputSettings(QWidget *parent = 0);
	~OutputSettings();

public slots:
	void on_outputEnable_toggled(bool state);
	void on_blankingEnable_toggled(bool state);
	void on_blankingInvert_toggled(bool state);
	void on_xEnable_toggled(bool state);
	void on_yEnable_toggled(bool state);
	void on_xInvert_toggled(bool state);
	void on_yInvert_toggled(bool state);
	void on_xySwap_toggled(bool state);
	void on_aspectRatio_currentIndexChanged(int index);
	void on_aspectScale_toggled(bool state);
	void on_fitSquare_toggled(bool state);
	void on_enforceSafety_toggled(bool state);
	void on_outputTest_pressed();
	void on_outputTest_released();
	void on_redPowerSlider_valueChanged(int value);
	void on_greenPowerSlider_valueChanged(int value);
	void on_bluePowerSlider_valueChanged(int value);
	void on_powerSlider_valueChanged(int value);
	void on_offsetSlider_valueChanged(int value);
	void on_delaySlider_valueChanged(int value);
	void on_sizeSlider_valueChanged(int value);
	void on_resetTransform_clicked();

	void resizeEvent (QResizeEvent * event);
	void showEvent (QShowEvent * event);
	
	void pointMoved(ControlPoint *pt);

private:
	QTransform mtx;
	QGraphicsScene scene;
	ControlPoint *pt[4];
	QGraphicsPolygonItem pl;
	int currentAspect;
	void updatePoly();
	void updateMatrix();
	void updateAllSettings();
	void resetDefaults();
	void resetPoints();
	void loadPoints();
	qreal getYRatio(int ratio);
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
