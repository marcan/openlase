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

#include <QApplication>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QDebug>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>

#include "qplayvid_gui.h"

#define Setting(name, ...) \
	addSetting(new PlayerSetting(this, #name, settings.name, __VA_ARGS__))

PlayerUI::PlayerUI(QWidget *parent)
	: QMainWindow(parent)
	, player(NULL)
	, playing(false)
{
	QWidget *central = new QWidget(this);
	QVBoxLayout *winbox = new QVBoxLayout();

	resize(800, 0);
	setWindowTitle("OpenLase Video Player");

	b_playstop = new QPushButton("Play");
	connect(b_playstop, SIGNAL(clicked(bool)), this, SLOT(playStopClicked()));
	b_pause = new QPushButton("Pause");
	b_pause->setCheckable(true);
	connect(b_pause, SIGNAL(toggled(bool)), this, SLOT(pauseToggled(bool)));
	b_step = new QPushButton("Skip");
	b_step->setEnabled(false);
	connect(b_step, SIGNAL(clicked(bool)), this, SLOT(stepClicked()));
	b_load = new QPushButton("Load");
	connect(b_load, SIGNAL(clicked(bool)), this, SLOT(loadSettings()));
	b_save = new QPushButton("Save");
	connect(b_save, SIGNAL(clicked(bool)), this, SLOT(saveSettings()));

	loadDefaults();

	sl_time = new QSlider(Qt::Horizontal);
	sl_time->setMinimum(0);
	sl_time->setMaximum(100000);
	connect(sl_time, SIGNAL(sliderPressed()), this, SLOT(timePressed()));
	connect(sl_time, SIGNAL(sliderReleased()), this, SLOT(timeReleased()));
	connect(sl_time, SIGNAL(sliderMoved(int)), this, SLOT(timeMoved(int)));

	winbox->addWidget(sl_time);

	QHBoxLayout *controlsbox = new QHBoxLayout();
	controlsbox->addWidget(b_playstop);
	controlsbox->addWidget(b_pause);
	controlsbox->addWidget(b_step);
	controlsbox->addWidget(b_load);
	controlsbox->addWidget(b_save);
	controlsbox->addWidget(new QLabel("Vol:"));
	controlsbox->addLayout(Setting(volume, 0, 100, 1, false));

	QHBoxLayout *settingsbox = new QHBoxLayout();
	QFormLayout *tracebox = new QFormLayout();
	QGroupBox *tracegroup = new QGroupBox("Tracing settings");
	QHBoxLayout *modebox = new QHBoxLayout();

	r_thresh = new QRadioButton("Threshold");
	r_canny = new QRadioButton("Canny");
	c_splitthresh = new QCheckBox("Split threshold");

	connect(r_thresh, SIGNAL(clicked(bool)), this, SLOT(modeChanged()));
	connect(r_canny, SIGNAL(clicked(bool)), this, SLOT(modeChanged()));
	connect(c_splitthresh, SIGNAL(clicked(bool)), this, SLOT(splitChanged()));

	modebox->addWidget(r_thresh);
	modebox->addWidget(r_canny);
	modebox->addWidget(c_splitthresh);
	tracebox->addRow("Mode", 			modebox);
	tracebox->addRow("Scale", 			Setting(scale, 		10, 100, 1));
	tracebox->addRow("Blur", 			Setting(blur, 		0, 500, 5));
	tracebox->addRow("Threshold", 		Setting(threshold, 	0, 500, 1));
	tracebox->addRow("Threshold 2", 	Setting(threshold2, 0, 500, 1));
	tracebox->addRow("Dark value", 		Setting(darkval, 	0, 255, 10));
	tracebox->addRow("Light value", 	Setting(lightval, 	0, 255, 10));
	tracebox->addRow("Border offset", 	Setting(offset, 	0, 50, 5));
	tracebox->addRow("Decimation", 		Setting(decimation, 1, 15, 1));
	splitChanged();
	tracegroup->setLayout(tracebox);

	QFormLayout *renderbox = new QFormLayout();
	QGroupBox *rendergroup = new QGroupBox("Render settings");
	renderbox->addRow("Min. size", 		Setting(minsize, 	0, 100, 1));
	renderbox->addRow("Start wait", 	Setting(startwait, 	0, 20, 1));
	renderbox->addRow("End wait", 		Setting(endwait, 	0, 20, 1));
	renderbox->addRow("Dwell", 			Setting(dwell, 		0, 20, 1));
	renderbox->addRow("Off speed", 		Setting(offspeed, 	10, 100, 1));
	renderbox->addRow("Snap", 			Setting(snap, 		0, 100, 1));
	renderbox->addRow("Min. rate", 		Setting(minrate, 	0, 40, 1));
	renderbox->addRow("Overscan", 		Setting(overscan, 	0, 100, 1));
	rendergroup->setLayout(renderbox);

	settingsbox->addWidget(tracegroup);
	settingsbox->addWidget(rendergroup);

	winbox->addLayout(controlsbox);
	winbox->addLayout(settingsbox);

	sb_fps = new QLabel();
	sb_afps = new QLabel();
	sb_objects = new QLabel();
	sb_points = new QLabel();
	sb_pts = new QLabel();

	statusBar()->addWidget(sb_fps);
	statusBar()->addWidget(sb_afps);
	statusBar()->addWidget(sb_objects);
	statusBar()->addWidget(sb_points);
	statusBar()->addWidget(sb_pts);

	updateSettingsUI();
	central->setLayout(winbox);
	setCentralWidget(central);
}

void PlayerUI::modeChanged()
{
	if (r_thresh->isChecked()) {
		settings.canny = false;
		findSetting("threshold")->setMaximum(255);
		findSetting("threshold2")->setMaximum(255);
		c_splitthresh->setEnabled(true);
		splitChanged();
	} else {
		settings.canny = true;
		c_splitthresh->setEnabled(false);
		findSetting("threshold")->setMaximum(500);
		findSetting("threshold2")->setMaximum(500);
		findSetting("threshold2")->setEnabled(true);
		findSetting("darkval")->setEnabled(false);
		findSetting("lightval")->setEnabled(false);
		findSetting("offset")->setEnabled(false);
	}
	updateSettings();
}

void PlayerUI::splitChanged()
{
	bool split = c_splitthresh->isChecked();
	findSetting("threshold2")->setEnabled(split);
	findSetting("darkval")->setEnabled(split);
	findSetting("lightval")->setEnabled(split);
	findSetting("offset")->setEnabled(split);
	settings.splitthreshold = split;
	updateSettings();
}

PlayerSetting *PlayerUI::findSetting(const QString &name)
{
	foreach(PlayerSetting *s, lsettings)
		if (s->name == name)
			return s;
	qDebug() << "Could not find setting:" << name;
	return NULL;
}

QLayout *PlayerUI::addSetting(PlayerSetting *setting)
{
	lsettings << setting;
	connect(setting, SIGNAL(valueChanged(int)), this, SLOT(updateSettings()));
	return setting->layout;
}

void PlayerUI::loadDefaults()
{
	settings.canny = 1;
	settings.splitthreshold = 0;
	settings.blur = 100;
	settings.scale = 100;
	settings.threshold = 30;
	settings.threshold2 = 20;
	settings.darkval = 96;
	settings.lightval = 160;
	settings.offset = 0;
	settings.decimation = 3;
	settings.minsize = 10;
	settings.startwait = 8;
	settings.endwait = 3;
	settings.dwell = 2;
	settings.offspeed = 50;
	settings.snap = 10;
	settings.minrate = 15;
	settings.overscan = 0;
	settings.volume = 50;
}

void PlayerUI::updateSettingsUI()
{

	foreach(PlayerSetting *s, lsettings)
		s->updateValue();

	c_splitthresh->setChecked(settings.splitthreshold);
	splitChanged();
	r_thresh->setChecked(!settings.canny);
	r_canny->setChecked(settings.canny);
	modeChanged();

	if (player)
		playvid_update_settings(player, &this->settings);
}

void PlayerUI::open(const char *filename)
{
	qDebug() << "open:" << filename;
	this->filename = filename;
	loadSettings();
	if(playvid_open(&player, filename)) {
		qDebug() << "open failed";
	}
	playvid_set_eventcb(player, player_event_cb);
	playvid_update_settings(player, &this->settings);
	sl_time->setMaximum((int)(1000 * playvid_get_duration(player)));
}

void PlayerUI::playStopClicked(void)
{
	if (!player)
		return;

	if (playing) {
		b_playstop->setText("Play");
		playvid_stop(player);
		playing = false;
	} else {
		b_playstop->setText("Stop");
		if (b_pause->isChecked())
			playvid_pause(player);
		else
			playvid_play(player);
		playing = true;
	}
}

void PlayerUI::pauseToggled(bool pause)
{
	if (!player)
		return;
	b_step->setEnabled(pause);
	if (playing) {
		if (pause)
			playvid_pause(player);
		else
			playvid_play(player);
	}
}

void PlayerUI::stepClicked(void)
{
	if (!player)
		return;

	if (playing && b_pause->isChecked()) {
		playvid_skip(player);
	}
}

void PlayerUI::updateSettings()
{
	if (player)
		playvid_update_settings(player, &this->settings);
}

void PlayerUI::playEvent(PlayerEvent *e)
{
	qDebug() << "Objects:" << e->objects;
	sb_fps->setText(QString("FPS: %1").arg(1.0/e->ftime, 0, 'f', 2));
	sb_afps->setText(QString("Avg: %1").arg(e->frames/e->time, 0, 'f', 2));
	sb_objects->setText(QString("Obj: %1").arg(e->objects));
	QString points = QString("Pts: %1").arg(e->points);
	if (e->padding_points)
		points += QString(" (pad %1)").arg(e->resampled_points);
	else if (e->resampled_points)
		points += QString(" (Rp %1 Bp %2)").arg(e->resampled_points).arg(e->resampled_blacks);
	sb_points->setText(points);
	if (!sl_time->isSliderDown()) {
		if (e->ended) {
			if (playing) {
				playvid_stop(player);
				playing = false;
				b_playstop->setText("Play");
				sl_time->setValue(0);
				playvid_seek(player, 0);
			}
		} else {
			sl_time->setValue((int)(e->pts * 1000));
		}
	}
}

void PlayerUI::timePressed()
{
	if (!player || !playing)
		return;
	if (!b_pause->isChecked())
		playvid_pause(player);
}

void PlayerUI::timeReleased()
{
	if (!player || !playing)
		return;
	if (!b_pause->isChecked()) {
		playvid_play(player);
	}
}

void PlayerUI::timeMoved(int value)
{
	if (!player)
		return;
	qDebug() << "timeMoved" << value;
	playvid_seek(player, value / 1000.0);
}

bool PlayerUI::event(QEvent *e)
{
	if (e->type() == QPlayerEvent::type) {
		QPlayerEvent *ev = dynamic_cast<QPlayerEvent*>(e);
		playEvent(&ev->data);
		return true;
	} else {
		return QMainWindow::event(e);
	}
}

void PlayerUI::loadSettings(void)
{
	QFile file(filename + ".cfg");
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
		if (name == "canny") {
			settings.canny = val;
		} else if (name == "splitthreshold") {
			settings.splitthreshold = val;
		} else {
			PlayerSetting *s = findSetting(name);
			if (!s)
				qDebug() << "Unknown setting" << name;
			else
				s->setValue(val);
		}
	}
	updateSettingsUI();
	file.close();
}

void PlayerUI::saveSettings(void)
{
	QFile file(filename + ".cfg");
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qDebug() << "Settings save failed";
		return;
	}
	QTextStream ts(&file);
	ts << QString("canny=%1\n").arg(settings.canny);
	ts << QString("splitthreshold=%1\n").arg(settings.splitthreshold);
	foreach(PlayerSetting *s, lsettings)
		ts << QString("%1=%2\n").arg(s->name).arg(s->value);
	ts.flush();
	file.close();
}

PlayerUI::~PlayerUI()
{
	if (player) {
		playvid_stop(player);
	}
}

PlayerSetting::PlayerSetting ( QObject* parent, const QString &name, int& value,
							   int min, int max, int step, bool addbox )
	: QObject (parent)
	, value(value)
	, name(name)
	, i_value(value)
	, enabled(true)
{
	layout = new QHBoxLayout;
	slider = new QSlider(Qt::Horizontal);
	slider->setMinimum(min);
	slider->setMaximum(max);
	slider->setSingleStep(step);
	slider->setPageStep(step * 10);
	slider->setValue(value);
	connect(slider, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
	layout->addWidget(slider);
	if (addbox) {
		spin = new QSpinBox;
		spin->setMinimum(min);
		spin->setMaximum(max);
		spin->setSingleStep(step);
		spin->setValue(value);
		connect(spin, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
		layout->addWidget(spin);
	} else {
		spin = NULL;
	}
}

void PlayerSetting::setValue(int newValue)
{
	if (newValue == this->i_value)
		return;
	this->i_value = newValue;
	updateValue();
	emit valueChanged(newValue);
}

void PlayerSetting::updateValue()
{
	slider->setValue(this->i_value);
	if (spin)
		spin->setValue(this->i_value);
}

void PlayerSetting::setEnabled(bool enabled)
{
	if (enabled == this->enabled)
		return;

	this->enabled = enabled;
	slider->setEnabled(enabled);
	if (spin)
		spin->setEnabled(enabled);
}

void PlayerSetting::setMaximum(int max)
{
	if (i_value > max)
		setValue(max);
	slider->setMaximum(max);
	if (spin)
		spin->setMaximum(max);
}

void PlayerSetting::setMinimum(int min)
{
	if (i_value < min)
		setValue(min);
	slider->setMinimum(min);
	if (spin)
		spin->setMinimum(min);
}

QPlayerEvent::QPlayerEvent(PlayerEvent data)
	: QEvent(type)
	, data(data)
{
}

QApplication *app;
PlayerUI *ui;

void player_event_cb(PlayerEvent *ev)
{
	QPlayerEvent *qev = new QPlayerEvent(*ev);
	app->postEvent(ui, qev);
}

int main (int argc, char *argv[])
{
	app = new QApplication(argc, argv);
	playvid_init();
	ui = new PlayerUI();
	ui->open(argv[1]);
	ui->show();
	int ret = app->exec();
	delete ui;
	delete app;
	return ret;
}
