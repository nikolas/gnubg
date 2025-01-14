/*
 * Copyright (C) 2002 Joern Thyssen <jthyssen@dk.ibm.com>
 * Copyright (C) 2002-2008 the AUTHORS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * $Id: sound.h,v 1.26 2020/01/05 19:51:24 plm Exp $
 */

#ifndef SOUND_H
#define SOUND_H

typedef enum {
    /* start & exit of gnubg */
    SOUND_START = 0,
    SOUND_EXIT,
    /* commands */
    SOUND_AGREE,
    SOUND_DOUBLE,
    SOUND_DROP,
    SOUND_CHEQUER,
    SOUND_MOVE,
    SOUND_REDOUBLE,
    SOUND_RESIGN,
    SOUND_ROLL,
    SOUND_TAKE,
    /* events */
    SOUND_HUMAN_DANCE,
    SOUND_HUMAN_WIN_GAME,
    SOUND_HUMAN_WIN_MATCH,
    SOUND_BOT_DANCE,
    SOUND_BOT_WIN_GAME,
    SOUND_BOT_WIN_MATCH,
    SOUND_ANALYSIS_FINISHED,
    /* number of sounds */
    NUM_SOUNDS
} gnubgsound;

extern const char *sound_description[NUM_SOUNDS];
extern const char *sound_command[NUM_SOUNDS];

extern int fSound;
extern int fQuiet;

extern void playSound(const gnubgsound gs);
extern void SoundWait(void);

extern char *GetDefaultSoundFile(gnubgsound sound);
extern void playSoundFile(char *file, gboolean sync);
extern void SetSoundFile(const gnubgsound sound, const char *file);
extern char *GetSoundFile(gnubgsound sound);
extern const char *sound_get_command(void);
extern char *sound_set_command(const char *sz);
extern void SetExitSoundOff(void);

#endif
