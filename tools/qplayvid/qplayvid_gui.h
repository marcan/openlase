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

#ifndef QPLAYVID_GUI_H
#define QPLAYVID_GUI_H

#include "qplayvid.h"

#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QLabel>
#include <qcoreevent.h>

class PlayerSetting : public QObject
{
	Q_OBJECT

public:
	PlayerSetting(QObject *parent, const QString &name, int& value,
				  int min, int max, int step = 1, bool addbox = true);
	const int& value;
	QLayout *layout;
	QString name;

	void updateValue();
	void setMinimum(int min);
	void setMaximum(int min);

signals:
	void valueChanged(int newValue);

public slots:
	void setValue(int value);
	void setEnabled(bool enabled);

private:
	int& i_value;
	bool enabled;
	QSlider *slider;
	QSpinBox *spin;
};

class QPlayerEvent : public QEvent
{
public:
	static const QEvent::Type type = QEvent::User;
	QPlayerEvent(PlayerEvent data);
	PlayerEvent data;
};

class PlayerUI : public QMainWindow
{
	Q_OBJECT

public:
	PlayerUI(QWidget *parent = 0);
	~PlayerUI();
	void open(const char *filename);
private:
	QString filename;
	PlayerCtx *player;
	bool playing;
	PlayerSettings settings;
	QList<PlayerSetting *> lsettings;

	QSlider *sl_time;

	QPushButton *b_playstop;
	QPushButton *b_pause;
	QPushButton *b_step;
	QPushButton *b_load;
	QPushButton *b_save;

	QRadioButton *r_thresh;
	QRadioButton *r_canny;
	QCheckBox *c_splitthresh;

	QLabel *sb_fps;
	QLabel *sb_afps;
	QLabel *sb_objects;
	QLabel *sb_points;
	QLabel *sb_pts;

	QLayout *addSetting(PlayerSetting *setting);
	PlayerSetting *findSetting(const QString &name);
	void loadDefaults(void);
	void playEvent(PlayerEvent *e);
	bool event(QEvent *e);
private slots:
	void modeChanged();
	void splitChanged();
	void updateSettings();
	void updateSettingsUI();
	void playStopClicked();
	void pauseToggled(bool pause);
	void stepClicked();
	void timePressed();
	void timeReleased();
	void timeMoved(int value);
	void loadSettings();
	void saveSettings();
};

void player_event_cb(PlayerEvent *ev);

#endif