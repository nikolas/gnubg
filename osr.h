/*
 * osr.h
 *
 * by Jørn Thyssen <jthyssen@dk.ibm.com>, 2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: osr.h,v 1.7 2012/08/27 23:06:41 plm Exp $
 */

#ifndef OSR_H
#define OSR_H

extern void
 raceProbs(const TanBoard anBoard, const unsigned int nGames, float arOutput[NUM_OUTPUTS], float arMu[2]);


#endif                          /* OSR_H */
