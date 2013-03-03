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

#include <QMessageBox>
#include <QGraphicsLineItem>

#include "output_settings.h"

#define ASPECT_1_1    0
#define ASPECT_4_3    1
#define ASPECT_16_9   2
#define ASPECT_3_2    3

#define COLORMODE_ANALOG 0
#define COLORMODE_TTL    1

#define COLORCHANNEL_RGB 0
#define COLORCHANNEL_R   1
#define COLORCHANNEL_G   2
#define COLORCHANNEL_B   3

ControlPoint::ControlPoint()
{
	form = 0;
}

QVariant ControlPoint::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemPositionChange && form) {
		form->pointMoved(this);
	}
	return QGraphicsEllipseItem::itemChange(change, value);
}

OutputSettings::OutputSettings(QWidget *parent)
{
	setupUi(this);
	
	QPen grid_pen(Qt::blue);
	QPen line_pen(Qt::green);
	QBrush point_brush(Qt::red);
	
	scene.setSceneRect(-1.1,-1.1,2.2,2.2);
	
	for (int i=-5; i<=5; i++) {
		scene.addLine(i/5.0f, -1.0f, i/5.0f, 1.0f, grid_pen);
		scene.addLine(-1.0f, i/5.0f, 1.0f, i/5.0f, grid_pen);
	}
	
	for (int i=0; i<4; i++) {
		pt[i] = new ControlPoint();
	}
	
	for (int i=0; i<4; i++) {
		pt[i]->setBrush(point_brush);
		pt[i]->setRect(-5, -5, 10, 10);
		pt[i]->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
		pt[i]->setFlag(QGraphicsItem::ItemIsMovable, true);
		pt[i]->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
		pt[i]->form = this;
		scene.addItem(pt[i]);
	}
	
	pl.setPen(line_pen);

	scene.addItem(&pl);

	projView->setBackgroundBrush(Qt::black);
	projView->setScene(&scene);
	projView->fitInView(scene.sceneRect(), Qt::KeepAspectRatio);
	projView->setInteractive(true);
	//projView->setRenderHints(QPainter::Antialiasing);

	resetDefaults();
}

OutputSettings::~OutputSettings()
{
}

qreal OutputSettings::getYRatio(int ratio)
{
	switch(ratio) {
		case ASPECT_1_1:
			return 1.0;
		case ASPECT_4_3:
			return 3.0/4.0;
		case ASPECT_16_9:
			return 9.0/16.0;
		case ASPECT_3_2:
			return 2.0/3.0;
	}
	return 1.0;
}

void OutputSettings::resetPoints()
{
	mtx.reset();
	mtx.scale(1.0, getYRatio(aspectRatio->currentIndex()));
	updateMatrix();
	loadPoints();
	updatePoly();
}

void OutputSettings::updateMatrix()
{
	QTransform smtx;
	QTransform omtx;
	qreal yratio = getYRatio(aspectRatio->currentIndex());
	
	if (!aspectScale->isChecked()) {
		if (fitSquare->isChecked())
			smtx.scale(yratio, 1.0);
		else
			smtx.scale(1.0, 1/yratio);
	}
	
	omtx = smtx * mtx;

	cfg.transform[0][0] = omtx.m11();
	cfg.transform[0][1] = omtx.m21();
	cfg.transform[0][2] = omtx.m31();
	cfg.transform[1][0] = omtx.m12();
	cfg.transform[1][1] = omtx.m22();
	cfg.transform[1][2] = omtx.m32();
	cfg.transform[2][0] = omtx.m13();
	cfg.transform[2][1] = omtx.m23();
	cfg.transform[2][2] = omtx.m33();
}

void OutputSettings::loadPoints()
{
	QPointF p0(-1,-1);
	QPointF p1(1,-1);
	QPointF p2(-1,1);
	QPointF p3(1,1);

	for (int i=0; i<4; i++)
		pt[i]->setFlag(QGraphicsItem::ItemSendsGeometryChanges, false);

	pt[0]->setPos(mtx.map(p0));
	pt[1]->setPos(mtx.map(p1));
	pt[2]->setPos(mtx.map(p2));
	pt[3]->setPos(mtx.map(p3));

	for (int i=0; i<4; i++)
		pt[i]->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

	updatePoly();
}

void OutputSettings::pointMoved(ControlPoint *point)
{
	QPolygonF src;
	src << QPoint(-1,-1) << QPoint(1,-1) << QPoint(1,1) << QPoint(-1,1);
	QPolygonF dst;
	dst << pt[0]->pos() << pt[1]->pos() << pt[3]->pos() << pt[2]->pos();

	QTransform::quadToQuad(src, dst, mtx);
	loadPoints();
	updatePoly();
	updateMatrix();
}

void OutputSettings::updatePoly()
{
	QPolygonF poly;
	
	poly << pt[0]->pos() << pt[1]->pos() << pt[3]->pos() << pt[2]->pos();
	pl.setPolygon(poly);
}

void OutputSettings::resizeEvent (QResizeEvent * event)
{
	projView->fitInView(scene.sceneRect(), Qt::KeepAspectRatio);
}

void OutputSettings::showEvent (QShowEvent * event)
{
	projView->fitInView(scene.sceneRect(), Qt::KeepAspectRatio);
}

void OutputSettings::updateAllSettings()
{
	cfg.power = powerBox->value() / 100.0f;
	cfg.size = sizeBox->value() / 100.0f;
	cfg.redMax = redMaxBox->value() / 100.0f;
	cfg.redMin = redMinBox->value() / 100.0f;
	cfg.redBlank = redBlankBox->value() / 100.0f;
	cfg.redDelay = redDelayBox->value();
	cfg.greenMax = redMaxBox->value() / 100.0f;
	cfg.greenMin = redMinBox->value() / 100.0f;
	cfg.greenBlank = redBlankBox->value() / 100.0f;
	cfg.greenDelay = redDelayBox->value();
	cfg.blueMax = redMaxBox->value() / 100.0f;
	cfg.blueMin = redMinBox->value() / 100.0f;
	cfg.blueBlank = redBlankBox->value() / 100.0f;
	cfg.blueDelay = redDelayBox->value();
	
	cfg.scan_flags = 0;
	cfg.blank_flags = 0;
	cfg.safe = enforceSafety->isChecked();

	if(cfg.safe) {
		xEnable->setChecked(true);
		yEnable->setChecked(true);
		cfg.scan_flags |= ENABLE_X | ENABLE_Y;
	}
	
	xEnable->setEnabled(!cfg.safe);
	yEnable->setEnabled(!cfg.safe);
	
	currentAspect = aspectRatio->currentIndex();

	if (xEnable->isChecked())
		cfg.scan_flags |= ENABLE_X;
	if (yEnable->isChecked())
		cfg.scan_flags |= ENABLE_Y;
	if (xInvert->isChecked())
		cfg.scan_flags |= INVERT_X;
	if (yInvert->isChecked())
		cfg.scan_flags |= INVERT_Y;
	if (xySwap->isChecked())
		cfg.scan_flags |= SWAP_XY;

	if (outputEnable->isChecked())
		cfg.blank_flags |= OUTPUT_ENABLE;
	if (blankingEnable->isChecked())
		cfg.blank_flags |= BLANK_ENABLE;

	outputTest->setEnabled(!outputEnable->isChecked());
}

void OutputSettings::resetDefaults()
{
	xEnable->setChecked(true);
	yEnable->setChecked(true);
	xInvert->setChecked(true);
	yInvert->setChecked(false);
	xySwap->setChecked(false);
	aspectScale->setChecked(false);
	
	enforceSafety->setChecked(true);
	
	aspectRatio->setCurrentIndex(ASPECT_1_1);

	outputEnable->setChecked(true);
	blankingEnable->setChecked(true);

	colorMode->setCurrentIndex(COLORMODE_ANALOG);
	colorChannels->setCurrentIndex(COLORCHANNEL_RGB);

	powerBox->setValue(100);
	sizeBox->setValue(100);

	redEnable->setChecked(true);
	redMaxBox->setValue(100);
	redMinBox->setValue(0);
	redBlankBox->setValue(0);
	redDelayBox->setValue(7);

	greenEnable->setChecked(true);
	greenMaxBox->setValue(100);
	greenMinBox->setValue(0);
	greenBlankBox->setValue(0);
	greenDelayBox->setValue(7);

	blueEnable->setChecked(true);
	blueMaxBox->setValue(100);
	blueMinBox->setValue(0);
	blueBlankBox->setValue(0);
	blueDelayBox->setValue(7);
	
	resetPoints();
	updateAllSettings();
}

void OutputSettings::on_outputEnable_toggled(bool state)
{
	if (state) cfg.blank_flags |= OUTPUT_ENABLE;
	else cfg.blank_flags &= ~OUTPUT_ENABLE;
	outputTest->setEnabled(!state);
}

void OutputSettings::on_blankingEnable_toggled(bool state)
{
	if (state) cfg.blank_flags |= BLANK_ENABLE;
	else cfg.blank_flags &= ~BLANK_ENABLE;
}

void OutputSettings::on_blankingInvert_toggled(bool state)
{
	if (state) cfg.blank_flags |= BLANK_INVERT;
	else cfg.blank_flags &= ~BLANK_INVERT;
}

void OutputSettings::on_outputTest_pressed()
{
	cfg.blank_flags |= OUTPUT_ENABLE;
}

void OutputSettings::on_outputTest_released()
{
	if (!outputEnable->isChecked())
		cfg.blank_flags &= ~OUTPUT_ENABLE;
}

void OutputSettings::on_xEnable_toggled(bool state)
{
	if (state) cfg.scan_flags |= ENABLE_X;
	else cfg.scan_flags &= ~ENABLE_X;
}

void OutputSettings::on_yEnable_toggled(bool state)
{
	if (state) cfg.scan_flags |= ENABLE_Y;
	else cfg.scan_flags &= ~ENABLE_Y;
}

void OutputSettings::on_xInvert_toggled(bool state)
{
	if (state) cfg.scan_flags |= INVERT_X;
	else cfg.scan_flags &= ~INVERT_X;
}

void OutputSettings::on_yInvert_toggled(bool state)
{
	if (state) cfg.scan_flags |= INVERT_Y;
	else cfg.scan_flags &= ~INVERT_Y;
}

void OutputSettings::on_xySwap_toggled(bool state)
{
	if (state) cfg.scan_flags |= SWAP_XY;
	else cfg.scan_flags &= ~SWAP_XY;
}

void OutputSettings::on_aspectScale_toggled(bool state)
{
	fitSquare->setEnabled(!state);
	updateMatrix();
}

void OutputSettings::on_fitSquare_toggled(bool state)
{
	updateMatrix();
}

void OutputSettings::on_aspectRatio_currentIndexChanged(int index)
{
	QTransform smtx;
	if (index == currentAspect)
		return;

	qreal rold = getYRatio(currentAspect);
	qreal rnew = getYRatio(index);
	
	smtx.scale(1.0, rnew/rold);
	mtx = smtx * mtx;
	
	currentAspect = index;
	loadPoints();
	updateMatrix();
}

void OutputSettings::on_colorMode_currentIndexChanged(int index)
{
	cfg.analogColor = (index == COLORMODE_ANALOG);
}

void OutputSettings::on_colorChannels_currentIndexChanged(int index)
{
	cfg.outputRed = (index == COLORCHANNEL_RGB || index == COLORCHANNEL_R);
	cfg.outputGreen = (index == COLORCHANNEL_RGB || index == COLORCHANNEL_G);
	cfg.outputBlue = (index == COLORCHANNEL_RGB || index == COLORCHANNEL_B);
}

void OutputSettings::on_resetTransform_clicked()
{
	resetPoints();
}

void OutputSettings::on_enforceSafety_toggled(bool state)
{
	if (!state) {
		QMessageBox msgBox;
		msgBox.setText("Do not stare into laser with remaining eye");
		msgBox.setInformativeText("Do you really want to disable safety enforcement?");
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		msgBox.setDefaultButton(QMessageBox::No);
		int ret = msgBox.exec();
		if (ret != QMessageBox::Yes) {
			enforceSafety->setChecked(true);
			return;
		}
	}
	
	if (state) {
		xEnable->setChecked(true);
		yEnable->setChecked(true);
		cfg.scan_flags |= ENABLE_X | ENABLE_Y;
	}
	
	xEnable->setEnabled(!state);
	yEnable->setEnabled(!state);
	
	cfg.safe = state;
}

void OutputSettings::on_powerBox_valueChanged(int value)
{
	cfg.power = value / 100.0f;
}

void OutputSettings::on_sizeBox_valueChanged(int value)
{
	cfg.size = value / 100.0f;
}

void OutputSettings::on_redEnable_toggled(bool state)
{
	cfg.redEnable = state;
}

void OutputSettings::on_redMaxBox_valueChanged(int value)
{
	cfg.redMax = value / 100.0f;
}

void OutputSettings::on_redMinBox_valueChanged(int value)
{
	cfg.redMin = value / 100.0f;
}

void OutputSettings::on_redBlankBox_valueChanged(int value)
{
	cfg.redBlank = value / 100.0f;
}

void OutputSettings::on_redDelayBox_valueChanged(int value)
{
	cfg.redDelay = value;
}

void OutputSettings::on_greenEnable_toggled(bool state)
{
	cfg.greenEnable = state;
}

void OutputSettings::on_greenMaxBox_valueChanged(int value)
{
	cfg.greenMax = value / 100.0f;
}

void OutputSettings::on_greenMinBox_valueChanged(int value)
{
	cfg.greenMin = value / 100.0f;
}

void OutputSettings::on_greenBlankBox_valueChanged(int value)
{
	cfg.greenBlank = value / 100.0f;
}

void OutputSettings::on_greenDelayBox_valueChanged(int value)
{
	cfg.greenDelay = value;
}

void OutputSettings::on_blueEnable_toggled(bool state)
{
	cfg.blueEnable = state;
}

void OutputSettings::on_blueMaxBox_valueChanged(int value)
{
	cfg.blueMax = value / 100.0f;
}

void OutputSettings::on_blueMinBox_valueChanged(int value)
{
	cfg.blueMin = value / 100.0f;
}

void OutputSettings::on_blueBlankBox_valueChanged(int value)
{
	cfg.blueBlank = value / 100.0f;
}

void OutputSettings::on_blueDelayBox_valueChanged(int value)
{
	cfg.blueDelay = value;
}


