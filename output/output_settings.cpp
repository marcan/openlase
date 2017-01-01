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
#include <QDebug>
#include <QTextStream>
#include <QFileDialog>

#include "output_settings.h"

#define ASPECT_1_1    0
#define ASPECT_4_3    1
#define ASPECT_16_9   2
#define ASPECT_3_2    3

#define SettingBit(field, name, bit) \
	addSetting(new OutputSetting(name, #name, cfg.field, bit))
#define Setting(name) \
	addSetting(new OutputSetting(name, #name, cfg.name))

ControlPoint::ControlPoint()
{
	form = 0;
}

QVariant ControlPoint::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemPositionHasChanged && form) {
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
	projView->setRenderHints(QPainter::Antialiasing);

	resetDefaults();

	Setting(power);
	Setting(scanSize);

	Setting(redMax);
	Setting(redMin);
	Setting(redBlank);
	Setting(redDelay);

	Setting(greenMax);
	Setting(greenMin);
	Setting(greenBlank);
	Setting(greenDelay);

	Setting(blueMax);
	Setting(blueMin);
	Setting(blueBlank);
	Setting(blueDelay);

	Setting(colorMode);

	SettingBit(scan_flags, xEnable, ENABLE_X);
	SettingBit(scan_flags, yEnable, ENABLE_Y);
	SettingBit(scan_flags, xInvert, INVERT_X);
	SettingBit(scan_flags, yInvert, INVERT_Y);
	SettingBit(scan_flags, xySwap, SWAP_XY);
	SettingBit(scan_flags, safe, SAFE);

	SettingBit(blank_flags, outputEnable, OUTPUT_ENABLE);
	SettingBit(blank_flags, blankingEnable, BLANK_ENABLE);
	SettingBit(blank_flags, blankingInvert, BLANK_INVERT);

	SettingBit(color_flags, redEnable, COLOR_RED);
	SettingBit(color_flags, greenEnable, COLOR_GREEN);
	SettingBit(color_flags, blueEnable, COLOR_BLUE);
	SettingBit(color_flags, monochrome, COLOR_MONOCHROME);

	xEnable->setEnabled(!(cfg.scan_flags & SAFE));
	yEnable->setEnabled(!(cfg.scan_flags & SAFE));

	connect(findSetting("safe"), SIGNAL(valueChanging(int&)), this, SLOT(safeToggled(int&)));
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

	qDebug() << pt[3]->pos();

	QTransform::quadToQuad(src, dst, mtx);
	//loadPoints();
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

void OutputSettings::updateSettings()
{
}

void OutputSettings::updateSettingsUI()
{
	foreach(OutputSetting *s, lsettings)
		s->updateValue();

}

OutputSetting *OutputSettings::findSetting(const QString &name)
{
	foreach(OutputSetting *s, lsettings)
		if (s->name == name)
			return s;
	qDebug() << "Could not find setting:" << name;
	return NULL;
}

void OutputSettings::addSetting(OutputSetting *setting)
{
	lsettings << setting;
	connect(setting, SIGNAL(valueChanged(int)), this, SLOT(updateSettings()));
}

void OutputSettings::resetDefaults()
{
	cfg.power = 100;
	cfg.scanSize = 100;

	cfg.redMax = 100;
	cfg.redMin = 0;
	cfg.redBlank = 0;
	cfg.redDelay = 0;

	cfg.greenMax = 100;
	cfg.greenMin = 0;
	cfg.greenBlank = 0;
	cfg.greenDelay = 0;

	cfg.blueMax = 100;
	cfg.blueMin = 0;
	cfg.blueBlank = 0;
	cfg.blueDelay = 0;

	cfg.colorMode = COLORMODE_ANALOG;
	cfg.scan_flags = ENABLE_X | ENABLE_Y | SAFE;
	cfg.blank_flags = OUTPUT_ENABLE | BLANK_ENABLE;
	cfg.color_flags = COLOR_RED | COLOR_GREEN | COLOR_BLUE;

	resetPoints();
	updateSettingsUI();
	updateSettings();
}

void OutputSettings::loadSettings(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qDebug() << "Settings load failed";
		return;
	}
	QTextStream ts(&file);
	QString line;
	while (!(line = ts.readLine()).isNull()) {
		QStringList l = line.split("=");
		if (l.length() != 2)
			continue;
		qDebug() << "Got setting" << l[0] << "value" << l[1];
		QString name = l[0];
		int val = l[1].toInt();
		{
			if (name == "aspectRatio") {
				aspectRatio->setCurrentIndex(val);
			} else if (name == "aspectScale") {
				aspectScale->setChecked(val);
			} else if (name == "fitSquare") {
				fitSquare->setChecked(val);
			} else if (name.startsWith("pt")) {
				int idx = name.mid(2).toInt();
				if (idx >= 0 && idx <= 3) {
					QStringList p = l[1].split(",");
					qDebug() << "pre setPos";
					pt[idx]->setPos(QPointF(p[0].toFloat(), p[1].toFloat()));
					qDebug() << "post setPos";
				}
				fitSquare->setChecked(val);
			} else {
				OutputSetting *s = findSetting(name);
				if (!s)
					qDebug() << "Unknown setting" << name;
				else
					s->setValue(val);
					s->updateValue();
			}
		}
	}

	updateSettings();
}

void OutputSettings::saveSettings(QString fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qDebug() << "Settings save failed";
		return;
	}
	QTextStream ts(&file);
	foreach(OutputSetting *s, lsettings)
		ts << QString("%1=%2\n").arg(s->name).arg(s->value);

	ts << QString("aspectRatio=%1\n").arg(aspectRatio->currentIndex());
	ts << QString("aspectScale=%1\n").arg((int)aspectScale->isChecked());
	ts << QString("fitSquare=%1\n").arg((int)fitSquare->isChecked());
	for (int i = 0; i < 4; i++) {
		ts << QString("pt%1=%2,%3\n")
			.arg(i).arg(pt[i]->pos().x()).arg(pt[i]->pos().y());
	}
	ts.flush();
	file.close();
}

void OutputSettings::on_actionLoadSettings_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this,
		"Load Settings", "", "Configuration Files (*.cfg)");

	loadSettings(fileName);
}

void OutputSettings::on_actionSaveSettings_triggered()
{
	QString fileName = QFileDialog::getSaveFileName(this,
		"Save Settings", "", "Configuration Files (*.cfg)");

	saveSettings(fileName);
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

void OutputSettings::on_resetTransform_clicked()
{
	resetPoints();
}

void OutputSettings::safeToggled(int &state)
{
	if (!state) {
		QMessageBox msgBox;
		msgBox.setText("Do not stare into laser with remaining eye");
		msgBox.setInformativeText("Do you really want to disable safety enforcement?");
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		msgBox.setDefaultButton(QMessageBox::No);
		int ret = msgBox.exec();
		if (ret != QMessageBox::Yes) {
			state = 1;
			safe->setChecked(true);
			return;
		}
	}

	if (state) {
		findSetting("xEnable")->setValue(1);
		findSetting("yEnable")->setValue(1);
	}

	findSetting("xEnable")->setEnabled(!state);
	findSetting("yEnable")->setEnabled(!state);
}

OutputSetting::OutputSetting ( QWidget* widget, const QString &name, int& value,
							   int bit )
	: QObject (widget)
	, bit(bit)
	, name(name)
	, widget(widget)
	, i_value(value)
{
	if (QSpinBox *spin = qobject_cast<QSpinBox *>(this->widget)) {
		connect(spin, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
	}
	if (QCheckBox *check = qobject_cast<QCheckBox *>(this->widget)) {
		connect(check, SIGNAL(toggled(bool)), this, SLOT(setState(bool)));
	}
	if (QComboBox *combo = qobject_cast<QComboBox *>(this->widget)) {
		connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(setValue(int)));
	}
	updateValue();
}

void OutputSetting::setValue(int newValue)
{
	if (newValue == value)
		return;
	value = newValue;
	emit valueChanging(newValue);
	updateTarget();
	emit valueChanged(newValue);
}

void OutputSetting::setState(bool newValue)
{
	setValue(newValue);
}

void OutputSetting::updateTarget()
{
	if (bit) {
		if (value)
			i_value |= bit;
		else
			i_value &= ~bit;
	} else {
		i_value = value;
	}
}

void OutputSetting::setEnabled(bool enabled)
{
	if (enabled == this->enabled)
		return;

	this->enabled = enabled;
	widget->setEnabled(enabled);
}

void OutputSetting::updateValue()
{
	if (bit)
		value = !!(i_value & bit);
	else
		value = i_value;

	if (QSpinBox *spin = qobject_cast<QSpinBox *>(widget)) {
		spin->setValue(value);
	};
	if (QCheckBox *check = qobject_cast<QCheckBox *>(widget)) {
		check->setChecked(!!value);
	}
	if (QComboBox *combo = qobject_cast<QComboBox *>(widget)) {
		combo->setCurrentIndex(value);
	}
}
