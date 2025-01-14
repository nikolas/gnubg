/*
 * Copyright (C) 2002-2003 Joern Thyssen <jth@gnubg.org>
 * Copyright (C) 2003-2022 the AUTHORS
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
 * $Id: gtktempmap.c,v 1.65 2022/09/02 13:43:30 plm Exp $
 */

/*
 * Based on Sho Sengoku's Equity Temperature Map
 * https://bkgm.com/articles/Sengoku/TemperatureMap/index.html 
 */

#include "config.h"

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "backgammon.h"
#include "eval.h"
#include "gtktempmap.h"
#include "gtkgame.h"
#include "drawboard.h"
#include "format.h"
#include "render.h"
#include "renderprefs.h"
#include "gtkboard.h"
#include "gtkwindows.h"
#include "gtkcube.h"

#define SIZE_QUADRANT 52


typedef struct {

    matchstate *pms;
    float aarEquity[6][6];
    float rAverage;

    GtkWidget *aapwDA[6][6];
    GtkWidget *aapwe[6][6];
    GtkWidget *pwAverage;

    GtkWidget *pweAverage;

    int aaanMove[6][6][8];

    gchar *szTitle;

} tempmap;


typedef struct {

    unsigned char *achDice[2];
    unsigned char *achPips[2];
    int fShowEquity;
    int fShowBestMove;
    int fInvert;
    GtkWidget *apwGauge[2];
    float rMin, rMax;

    tempmap *atm;
    int n;

    int nSizeDie;

} tempmapwidget;

/* Retain these from one GTKShowTempMap() to the next */
static int fShowEquity = FALSE;
static int fShowBestMove = FALSE;

static int
TempMapEquities(evalcontext * pec, const matchstate * pms,
                float aarEquity[6][6], int aaanMove[6][6][8], const gchar * szTitle, const float rFac)
{

    int i, j;
    float arOutput[NUM_ROLLOUT_OUTPUTS];
    TanBoard anBoard;
    int aaan[6][6][8];
    float aar[6][6];
    cubeinfo ci;
    cubeinfo cix;

    /* calculate equities */

    GetMatchStateCubeInfo(&cix, pms);

    if (szTitle && *szTitle) {
        gchar *sz = g_strdup_printf(_("Calculating equities for %s"), szTitle);
        ProgressStartValue(sz, 21);
        g_free(sz);
    } else
        ProgressStartValue(_("Calculating equities"), 21);

    for (i = 0; i < 6; ++i)
        for (j = 0; j <= i; ++j) {

            memcpy(&ci, &cix, sizeof ci);

            /* find best move */

            memcpy(anBoard, pms->anBoard, sizeof(anBoard));

            if (FindBestMove(aaan[i][j], i + 1, j + 1, anBoard, &ci, pec, defaultFilters) < 0) {
                ProgressEnd();
                return -1;
            }

            /* evaluate resulting position */

            SwapSides(anBoard);
            ci.fMove = !ci.fMove;

            if (GeneralEvaluationE(arOutput, (ConstTanBoard) anBoard, &ci, pec) < 0) {
                ProgressEnd();
                return -1;
            }

            InvertEvaluationR(arOutput, &cix);

            if (!cix.nMatchTo && rFac != 1.0f)
                arOutput[OUTPUT_CUBEFUL_EQUITY] *= rFac;

            aar[i][j] = arOutput[OUTPUT_CUBEFUL_EQUITY];
            aar[j][i] = arOutput[OUTPUT_CUBEFUL_EQUITY];

            if (i != j)
                memcpy(aaan[j][i], aaan[i][j], sizeof aaan[0][0]);

            ProgressValueAdd(1);

        }

    ProgressEnd();

    memcpy(aarEquity, aar, sizeof aar);
    memcpy(aaanMove, aaan, sizeof aaan);

    return 0;

}


static int
CalcTempMapEquities(evalcontext * pec, tempmapwidget * ptmw)
{

    int i;

    for (i = 0; i < ptmw->n; ++i)
        if (TempMapEquities(pec, ptmw->atm[i].pms,
                            ptmw->atm[i].aarEquity,
                            ptmw->atm[i].aaanMove,
                            ptmw->atm[i].szTitle, (float) (ptmw->atm[i].pms->nCube / ptmw->atm[0].pms->nCube)) < 0)
            return -1;

    return 0;

}




static void
UpdateStyle(GtkWidget * pw, const float r)
{

    GtkStyle *ps = gtk_style_copy(gtk_widget_get_style(pw));
    double *gbval;

    gbval = g_malloc(sizeof(*gbval));
    *gbval = 1.0 - (double)r;
    g_object_set_data_full(G_OBJECT(pw), "gbval", gbval, g_free);

    ps->bg[GTK_STATE_NORMAL].red = 0xFFFF;
    ps->bg[GTK_STATE_NORMAL].blue = ps->bg[GTK_STATE_NORMAL].green = (guint16) ((1.0f - r) * 0xFFFF);

    gtk_widget_set_style(pw, ps);

}


static void
SetStyle(GtkWidget * pw, const float rEquity, const float rMin, const float rMax, const int fInvert)
{

    float r = (rEquity - rMin) / (rMax - rMin);
    UpdateStyle(pw, fInvert ? (1.0f - r) : r);

}


static char *
GetEquityString(const float rEquity, const cubeinfo * pci, const int fInvert)
{

    float r;

    if (fInvert) {
        /* invert equity */
        if (pci->nMatchTo)
            r = 1.0f - rEquity;
        else
            r = -rEquity;
    } else
        r = rEquity;

    if (fInvert) {
        cubeinfo ci;
        memcpy(&ci, pci, sizeof ci);
        ci.fMove = !ci.fMove;

        return OutputMWC(r, &ci, TRUE);
    } else
        return OutputMWC(r, pci, TRUE);

}


static void
UpdateTempMapEquities(tempmapwidget * ptmw)
{

    int i, j;
    float rMax, rMin, r;
    cubeinfo ci;
    int m;
    char szMove[FORMATEDMOVESIZE];

    /* calc. min, max and average */

    rMax = -10000;
    rMin = +10000;
    for (m = 0; m < ptmw->n; ++m) {
        ptmw->atm[m].rAverage = 0.0f;
        for (i = 0; i < 6; ++i)
            for (j = 0; j < 6; ++j) {
                r = ptmw->atm[m].aarEquity[i][j];
                ptmw->atm[m].rAverage += r;
                if (r > rMax)
                    rMax = r;
                if (r < rMin)
                    rMin = r;
            }
        ptmw->atm[m].rAverage /= 36.0f;
    }

    ptmw->rMax = rMax;
    ptmw->rMin = rMin;

    /* update styles */

    GetMatchStateCubeInfo(&ci, ptmw->atm[0].pms);

    for (m = 0; m < ptmw->n; ++m) {
        for (i = 0; i < 6; ++i)
            for (j = 0; j < 6; ++j) {

                gchar *sz = g_strdup_printf("%s [%s]",
                                            GetEquityString(ptmw->atm[m].aarEquity[i][j],
                                                            &ci, ptmw->fInvert),
                                            FormatMove(szMove, (ConstTanBoard) ptmw->atm[m].pms->anBoard,
                                                       ptmw->atm[m].aaanMove[i][j]));

                SetStyle(ptmw->atm[m].aapwDA[i][j], ptmw->atm[m].aarEquity[i][j], rMin, rMax, ptmw->fInvert);

                gtk_widget_set_tooltip_text(ptmw->atm[m].aapwe[i][j], sz);
                g_free(sz);
                gtk_widget_queue_draw(ptmw->atm[m].aapwDA[i][j]);

            }

        SetStyle(ptmw->atm[m].pwAverage, ptmw->atm[m].rAverage, rMin, rMax, ptmw->fInvert);

        gtk_widget_set_tooltip_text(ptmw->atm[m].pweAverage,
                                    GetEquityString(ptmw->atm[m].rAverage, &ci, ptmw->fInvert));
        gtk_widget_queue_draw(ptmw->atm[m].pwAverage);

    }

    /* update labels on gauge */

    gtk_label_set_text(GTK_LABEL(ptmw->apwGauge[ptmw->fInvert]), GetEquityString(rMin, &ci, ptmw->fInvert));
    gtk_label_set_text(GTK_LABEL(ptmw->apwGauge[!ptmw->fInvert]), GetEquityString(rMax, &ci, ptmw->fInvert));
}

static gboolean
DrawQuadrant(GtkWidget * pw, cairo_t * cr, tempmapwidget * ptmw)
{
    const int *pi = (int *) g_object_get_data(G_OBJECT(pw), "user_data");
    int i = 0;
    int j = 0;
    int m = 0;
    cubeinfo ci;
    PangoLayout *layout;
    PangoFontDescription *description;
    float y;
    GString *str;
    char *pch, *tmp;
    GtkAllocation allocation;
    gtk_widget_get_allocation(pw, &allocation);

#if GTK_CHECK_VERSION(3,0,0)
    double *gbval = g_object_get_data(G_OBJECT(pw), "gbval");
    guint width, height;

    width = gtk_widget_get_allocated_width(pw);
    height = gtk_widget_get_allocated_height(pw);

    cairo_rectangle(cr, 0, 0, width, height);
    cairo_set_source_rgb(cr, 1.0, *gbval, *gbval);
    cairo_fill(cr);

    gtk_render_frame(gtk_widget_get_style_context(pw), cr, 0, 0, width, height);
#else
    (void) cr;                  /* silence compiler warning */
    gtk_paint_box(gtk_widget_get_style(pw), gtk_widget_get_window(pw), GTK_STATE_NORMAL,
                  GTK_SHADOW_IN, NULL, NULL, NULL, 0, 0, allocation.width, allocation.height);
#endif
    if (pi == NULL)
        return TRUE;

    if (*pi >= 0) {
        i = (*pi % 100) / 6;
        j = (*pi % 100) % 6;
        m = *pi / 100;
    } else {
        m = -(*pi + 1);
        j = -1;
    }

    str = g_string_new("");

    if (ptmw->fShowEquity) {
        float r = 0.0f;

        if (j >= 0)
            r = ptmw->atm[m].aarEquity[i][j];
        else if (j == -1)
            r = ptmw->atm[m].rAverage;
        GetMatchStateCubeInfo(&ci, ptmw->atm[0].pms);
        tmp = GetEquityString(r, &ci, ptmw->fInvert);
        while (*tmp == ' ')
            tmp++;
        g_string_append(str, tmp);
    }


    /* move */

    if (j >= 0 && ptmw->fShowBestMove) {
        char szMove[FORMATEDMOVESIZE];

        FormatMovePlain(szMove, (ConstTanBoard)ptmw->atm[m].pms->anBoard, ptmw->atm[m].aaanMove[i][j]);
        if (ptmw->fShowEquity)
            g_string_append_printf(str, " %s", szMove);
        else
            g_string_append(str, szMove);
    }

    if (str->len == 0) {
        g_string_free(str, TRUE);
        return TRUE;
    }

    pch = str->str;
    if (ptmw->fShowEquity && j >= 0 && ptmw->fShowBestMove)
        y = 2;
    else if (ptmw->fShowEquity)
        y = (float)(allocation.height - 4) / 2.0f;
    else
        y = 2 + (float)(allocation.height - 4) / 10.0f;

    description = pango_font_description_from_string("sans");
    pango_font_description_set_size(description, allocation.height * PANGO_SCALE / 8);
    layout = gtk_widget_create_pango_layout(pw, NULL);
    pango_layout_set_font_description(layout, description);
    do {
        tmp = strchr(pch, ' ');
        if (tmp)
            *tmp = 0;
        pango_layout_set_text(layout, pch, -1);
#if GTK_CHECK_VERSION(3,0,0)
        gtk_render_layout(gtk_widget_get_style_context(pw), cr, 2.0, (double)y, layout);
#else
        gtk_paint_layout(gtk_widget_get_style(pw), gtk_widget_get_window(pw),
                                GTK_STATE_NORMAL, TRUE, NULL, pw, NULL, 2, (int) y, layout);
#endif
        if (tmp) {
            pch = tmp + 1;
            y += (float)(allocation.height - 4) / 5.0f;
        }
    } while (tmp);

    g_object_unref(layout);
    g_string_free(str, TRUE);

    return TRUE;
}

#if ! GTK_CHECK_VERSION(3,0,0)
static void
ExposeQuadrant(GtkWidget * pw, GdkEventExpose * UNUSED(pev), tempmapwidget * ptmw)
{
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(pw));
    DrawQuadrant(pw, cr, ptmw);
    cairo_destroy(cr);
}
#endif

static void
ExposeDieArea(GtkWidget * pw, cairo_t * cr, gint area_x, gint area_y, gint area_width, gint area_height, tempmapwidget * ptmw)
{
    const int *pi = (int *) g_object_get_data(G_OBJECT(pw), "user_data");
    int x, y;
    int nSizeDie;
    GtkAllocation allocation;
    gtk_widget_get_allocation(pw, &allocation);

    nSizeDie = (allocation.width - 4) / 7;
    if (nSizeDie > ((allocation.height - 4) / 7))
        nSizeDie = (allocation.height - 4) / 7;

    if (ptmw->nSizeDie != nSizeDie) {
        int i;
        renderdata rd;

        /* render die */

        CopyAppearance(&rd);
        rd.nSize = ptmw->nSizeDie = nSizeDie;
#if defined(USE_BOARD3D)
        Copy3dDiceColour(&rd);
#endif

        for (i = 0; i < 2; ++i) {
            g_free(ptmw->achDice[i]);
            g_free(ptmw->achPips[i]);

            ptmw->achDice[i] = (unsigned char *) g_malloc(nSizeDie * nSizeDie * 7 * 7 * 4);
            ptmw->achPips[i] = (unsigned char *) g_malloc(nSizeDie * nSizeDie * 3);
        }

        RenderDice(&rd, ptmw->achDice[0], ptmw->achDice[1], nSizeDie * 7 * 4, FALSE);
        RenderPips(&rd, ptmw->achPips[0], ptmw->achPips[1], nSizeDie * 3);
    }

    x = (allocation.width - ptmw->nSizeDie * 7) / 2;
    y = (allocation.height - ptmw->nSizeDie * 7) / 2;

    cairo_save(cr);
    cairo_rectangle(cr, area_x, area_y, area_width, area_height);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_fill(cr);
    cairo_restore(cr);
    DrawDie(cr, ptmw->achDice, ptmw->achPips, ptmw->nSizeDie, x, y, ptmw->atm[0].pms->fMove, *pi + 1, FALSE);
}

#if GTK_CHECK_VERSION(3,0,0)
static gboolean
ExposeDie(GtkWidget * pw, cairo_t * cr, tempmapwidget * ptmw)
{
    ExposeDieArea(pw, cr, 3, 3, gtk_widget_get_allocated_width(pw) - 6, gtk_widget_get_allocated_height(pw) - 6, ptmw);
    return TRUE;
}
#else
static void
ExposeDie(GtkWidget * pw, GdkEventExpose * pev, tempmapwidget * ptmw)
{
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(pw));
    ExposeDieArea(pw, cr, pev->area.x, pev->area.y, pev->area.width, pev->area.height, ptmw);
    cairo_destroy(cr);
}
#endif

static void
TempMapPlyToggled(GtkWidget * pw, tempmapwidget * ptmw)
{
    const int *pi = (int *) g_object_get_data(G_OBJECT(pw), "user_data");

    evalcontext ec = { TRUE, 0, FALSE, TRUE, 0.0 };

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw))) {

        /* recalculate equities */

        ec.nPlies = *pi;

        if (CalcTempMapEquities(&ec, ptmw))
            return;

        UpdateTempMapEquities(ptmw);

    }

}


static void
ShowEquityToggled(GtkWidget * pw, tempmapwidget * ptmw)
{

    int f = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));

    if (f != ptmw->fShowEquity) {
        fShowEquity = ptmw->fShowEquity = f;
        UpdateTempMapEquities(ptmw);
    }

}


static void
ShowBestMoveToggled(GtkWidget * pw, tempmapwidget * ptmw)
{

    int f = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));

    if (f != ptmw->fShowBestMove) {
        fShowBestMove = ptmw->fShowBestMove = f;
        UpdateTempMapEquities(ptmw);
    }
}

static void
DestroyDialog(gpointer p, GObject * UNUSED(obj))
{
    tempmapwidget *ptmw = (tempmapwidget *) p;
    int i;

    /* garbage collect */

    g_free(ptmw->achDice[0]);
    g_free(ptmw->achDice[1]);
    g_free(ptmw->achPips[0]);
    g_free(ptmw->achPips[1]);

    for (i = 0; i < ptmw->n; ++i) {
        g_free(ptmw->atm[i].pms);
        g_free(ptmw->atm[i].szTitle);
    }

    g_free(ptmw->atm);

    g_free(ptmw);

}

extern void
GTKShowTempMap(const matchstate ams[], const int n, gchar * aszTitle[], const int fInvert)
{

    evalcontext ec = { TRUE, 0, FALSE, TRUE, 0.0 };

    tempmapwidget *ptmw;
    int *pi;
    int i, j;

    GtkWidget *pwDialog;
    GtkWidget *pwv;
    GtkWidget *pw;
    GtkWidget *pwh;
    GtkWidget *pwx = NULL;
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget *pwOuterGrid;
    GtkWidget *pwGrid = NULL;
#else
    GtkWidget *pwOuterTable;
    GtkWidget *pwTable = NULL;
#endif

    int k, l, km, lm, m;

    /* dialog */
    if (!cubeTempMapAtMoney) {
        pwDialog = GTKCreateDialog(_("Sho Sengoku Temperature Map - Distribution of Rolls"),
                               DT_INFO, NULL, DIALOG_FLAG_MODAL, NULL, NULL);
    } else {
        pwDialog = GTKCreateDialog(_("Temperature Map in Hypothetical Money Play"),
                               DT_INFO, NULL, DIALOG_FLAG_MODAL, NULL, NULL);
    }
                               

    ptmw = (tempmapwidget *) g_malloc(sizeof(tempmapwidget));
    ptmw->fShowBestMove = fShowBestMove;
    ptmw->fShowEquity = fShowEquity;
    ptmw->fInvert = fInvert;
    ptmw->n = n;
    ptmw->nSizeDie = -1;
    ptmw->achDice[0] = ptmw->achDice[1] = NULL;
    ptmw->achPips[0] = ptmw->achPips[1] = NULL;

    ptmw->atm = (tempmap *) g_malloc(n * sizeof(tempmap));
    for (i = 0; i < n; ++i) {
        ptmw->atm[i].pms = (matchstate *) g_malloc(sizeof(matchstate));
        memcpy(ptmw->atm[i].pms, &ams[i], sizeof(matchstate));
    }

    /* vbox to hold tree widget and buttons */

#if GTK_CHECK_VERSION(3,0,0)
    pwv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
#else
    pwv = gtk_vbox_new(FALSE, 6);
#endif
    gtk_container_set_border_width(GTK_CONTAINER(pwv), 6);
    gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), pwv);

    /* calculate number of rows and columns */

    for (lm = 1; /**/; ++lm)
        if (lm * lm >= n)
            break;

    for (km = 1; km * lm < n; ++km);

#if GTK_CHECK_VERSION(3,0,0)
    pwOuterGrid = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(pwOuterGrid), TRUE);
    gtk_grid_set_row_homogeneous(GTK_GRID(pwOuterGrid), TRUE);
    gtk_box_pack_start(GTK_BOX(pwv), pwOuterGrid, TRUE, TRUE, 0);
#else
    pwOuterTable = gtk_table_new(km, lm, TRUE);
    gtk_box_pack_start(GTK_BOX(pwv), pwOuterTable, TRUE, TRUE, 0);
#endif

    for (k = m = 0; k < km; ++k)
        for (l = 0; l < lm && m < n; ++l, ++m) {

            tempmap *ptm = &ptmw->atm[m];

            ptm->szTitle = (aszTitle && aszTitle[m] && *aszTitle[m]) ? g_strdup(aszTitle[m]) : NULL;

            pw = gtk_frame_new(ptm->szTitle);

#if GTK_CHECK_VERSION(3,0,0)
            gtk_grid_attach(GTK_GRID(pwOuterGrid), pw, l, k, 1, 1);

            pwGrid = gtk_grid_new();
            gtk_grid_set_column_homogeneous(GTK_GRID(pwGrid), TRUE);
            gtk_grid_set_row_homogeneous(GTK_GRID(pwGrid), TRUE);
            gtk_container_add(GTK_CONTAINER(pw), pwGrid);
#else
            gtk_table_attach_defaults(GTK_TABLE(pwOuterTable), pw, l, l + 1, k, k + 1);

            pwTable = gtk_table_new(7, 7, TRUE);
            gtk_container_add(GTK_CONTAINER(pw), pwTable);
#endif

            /* drawing areas */

            for (i = 0; i < 6; ++i) {
                for (j = 0; j < 6; ++j) {

                    ptm->aapwDA[i][j] = gtk_drawing_area_new();
                    ptm->aapwe[i][j] = gtk_event_box_new();
                    gtk_event_box_set_visible_window(GTK_EVENT_BOX(ptm->aapwe[i][j]), FALSE);

                    gtk_container_add(GTK_CONTAINER(ptm->aapwe[i][j]), ptm->aapwDA[i][j]);

                    gtk_widget_set_size_request(ptm->aapwDA[i][j], SIZE_QUADRANT, SIZE_QUADRANT);

#if GTK_CHECK_VERSION(3,0,0)
                    gtk_grid_attach(GTK_GRID(pwGrid), ptm->aapwe[i][j], i + 1, j + 1, 1, 1);
#else
                    gtk_table_attach_defaults(GTK_TABLE(pwTable), ptm->aapwe[i][j], i + 1, i + 2, j + 1, j + 2);
#endif

                    pi = (int *) g_malloc(sizeof(int));
                    *pi = i * 6 + j + m * 100;

                    g_object_set_data_full(G_OBJECT(ptm->aapwDA[i][j]), "user_data", pi, g_free);
#if GTK_CHECK_VERSION(3,0,0)
                    gtk_style_context_add_class(gtk_widget_get_style_context(ptm->aapwDA[i][j]), "gnubg-temp-map-quadrant");
                    g_signal_connect(G_OBJECT(ptm->aapwDA[i][j]), "draw", G_CALLBACK(DrawQuadrant), ptmw);
#else
                    g_signal_connect(G_OBJECT(ptm->aapwDA[i][j]), "expose_event", G_CALLBACK(ExposeQuadrant), ptmw);
#endif
                }

                /* die */

                pw = gtk_drawing_area_new();
                gtk_widget_set_size_request(pw, SIZE_QUADRANT, SIZE_QUADRANT);

#if GTK_CHECK_VERSION(3,0,0)
                gtk_grid_attach(GTK_GRID(pwGrid), pw, 0, i + 1, 1, 1);
#else
                gtk_table_attach_defaults(GTK_TABLE(pwTable), pw, 0, 1, i + 1, i + 2);
#endif

                pi = (int *) g_malloc(sizeof(int));
                *pi = i;

                g_object_set_data_full(G_OBJECT(pw), "user_data", pi, g_free);

#if GTK_CHECK_VERSION(3,0,0)
                g_signal_connect(G_OBJECT(pw), "draw", G_CALLBACK(ExposeDie), ptmw);
#else
                g_signal_connect(G_OBJECT(pw), "expose_event", G_CALLBACK(ExposeDie), ptmw);
#endif
                /* die */

                pw = gtk_drawing_area_new();
                gtk_widget_set_size_request(pw, SIZE_QUADRANT, SIZE_QUADRANT);

#if GTK_CHECK_VERSION(3,0,0)
                gtk_grid_attach(GTK_GRID(pwGrid), pw, i + 1, 0, 1, 1);
#else
                gtk_table_attach_defaults(GTK_TABLE(pwTable), pw, i + 1, i + 2, 0, 1);
#endif

                pi = (int *) g_malloc(sizeof(int));
                *pi = i;

                g_object_set_data_full(G_OBJECT(pw), "user_data", pi, g_free);

#if GTK_CHECK_VERSION(3,0,0)
                g_signal_connect(G_OBJECT(pw), "draw", G_CALLBACK(ExposeDie), ptmw);
#else
                g_signal_connect(G_OBJECT(pw), "expose_event", G_CALLBACK(ExposeDie), ptmw);
#endif

            }

            /* drawing area for average */

            ptm->pwAverage = gtk_drawing_area_new();
            ptm->pweAverage = gtk_event_box_new();
            gtk_event_box_set_visible_window(GTK_EVENT_BOX(ptm->pweAverage), FALSE);

            gtk_container_add(GTK_CONTAINER(ptm->pweAverage), ptm->pwAverage);

            gtk_widget_set_size_request(ptm->pwAverage, SIZE_QUADRANT, SIZE_QUADRANT);

#if GTK_CHECK_VERSION(3,0,0)
            gtk_grid_attach(GTK_GRID(pwGrid), ptm->pweAverage, 0, 0, 1, 1);
#else
            gtk_table_attach_defaults(GTK_TABLE(pwTable), ptm->pweAverage, 0, 1, 0, 1);
#endif

            pi = (int *) g_malloc(sizeof(int));
            *pi = -m - 1;

            g_object_set_data_full(G_OBJECT(ptm->pwAverage), "user_data", pi, g_free);

#if GTK_CHECK_VERSION(3,0,0)
            gtk_style_context_add_class(gtk_widget_get_style_context(ptm->pwAverage), "gnubg-temp-map-quadrant");
            g_signal_connect(G_OBJECT(ptm->pwAverage), "draw", G_CALLBACK(DrawQuadrant), ptmw);
#else
            g_signal_connect(G_OBJECT(ptm->pwAverage), "expose_event", G_CALLBACK(ExposeQuadrant), ptmw);
#endif

        }

    /* separator */

#if GTK_CHECK_VERSION(3,0,0)
    gtk_box_pack_start(GTK_BOX(pwv), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
#else
    gtk_box_pack_start(GTK_BOX(pwv), gtk_hseparator_new(), FALSE, FALSE, 0);
#endif

    /* gauge */

#if GTK_CHECK_VERSION(3,0,0)
    pwGrid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(pwv), pwGrid, FALSE, FALSE, 0);
#else
    pwTable = gtk_table_new(2, 16, FALSE);
    gtk_box_pack_start(GTK_BOX(pwv), pwTable, FALSE, FALSE, 0);
#endif

    for (i = 0; i < 16; ++i) {

        pw = gtk_drawing_area_new();
        gtk_widget_set_size_request(pw, 15, 20);

#if GTK_CHECK_VERSION(3,0,0)
        gtk_grid_attach(GTK_GRID(pwGrid), pw, i, 1, 1, 1);
        gtk_widget_set_hexpand(pw, TRUE);
#else
        gtk_table_attach_defaults(GTK_TABLE(pwTable), pw, i, i + 1, 1, 2);
#endif

        g_object_set_data(G_OBJECT(pw), "user_data", NULL);

#if GTK_CHECK_VERSION(3,0,0)
        gtk_style_context_add_class(gtk_widget_get_style_context(pw), "gnubg-temp-map-quadrant");
        g_signal_connect(G_OBJECT(pw), "draw", G_CALLBACK(DrawQuadrant), NULL);
#else
        g_signal_connect(G_OBJECT(pw), "expose_event", G_CALLBACK(ExposeQuadrant), NULL);
#endif

        UpdateStyle(pw, (float)i / 15.0f);


    }

    for (i = 0; i < 2; ++i) {
        ptmw->apwGauge[i] = gtk_label_new("");
#if GTK_CHECK_VERSION(3,0,0)
        gtk_grid_attach(GTK_GRID(pwGrid), ptmw->apwGauge[i], 15 * i, 0, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(pwTable), ptmw->apwGauge[i], 15 * i, 15 * i + 1, 0, 1);
#endif
    }

    /* separator */

#if GTK_CHECK_VERSION(3,0,0)
    gtk_box_pack_start(GTK_BOX(pwv), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
#else
    gtk_box_pack_start(GTK_BOX(pwv), gtk_hseparator_new(), FALSE, FALSE, 0);
#endif

    /* buttons */

#if GTK_CHECK_VERSION(3,0,0)
    pwh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
    pwh = gtk_hbox_new(FALSE, 4);
#endif
    gtk_box_pack_start(GTK_BOX(pwv), pwh, FALSE, FALSE, 0);

    for (i = 0; i < 4; ++i) {

        gchar *sz = g_strdup_printf(_("%d ply"), i);
        if (i == 0)
            pw = pwx = gtk_radio_button_new_with_label(NULL, sz);
        else
            pw = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pwx), sz);
        g_free(sz);

        gtk_box_pack_start(GTK_BOX(pwh), pw, FALSE, FALSE, 0);

        pi = (int *) g_malloc(sizeof(int));
        *pi = i;

        g_object_set_data_full(G_OBJECT(pw), "user_data", pi, g_free);

        g_signal_connect(G_OBJECT(pw), "toggled", G_CALLBACK(TempMapPlyToggled), ptmw);

    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pwx), TRUE);


    /* show-buttons */

    if (n < 2) {
#if GTK_CHECK_VERSION(3,0,0)
        pwh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
        pwh = gtk_hbox_new(FALSE, 4);
#endif
        gtk_box_pack_start(GTK_BOX(pwv), pwh, FALSE, FALSE, 0);
    }

    pw = gtk_check_button_new_with_label(_("Show equities"));
    gtk_toggle_button_set_active((GtkToggleButton *) pw, ptmw->fShowEquity);
    gtk_box_pack_end(GTK_BOX(pwh), pw, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(pw), "toggled", G_CALLBACK(ShowEquityToggled), ptmw);


    pw = gtk_check_button_new_with_label(_("Show best move"));
    gtk_toggle_button_set_active((GtkToggleButton *) pw, ptmw->fShowBestMove);
    gtk_box_pack_end(GTK_BOX(pwh), pw, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(pw), "toggled", G_CALLBACK(ShowBestMoveToggled), ptmw);


    /* update */

    CalcTempMapEquities(&ec, ptmw);
    UpdateTempMapEquities(ptmw);

    /* modality */

    gtk_window_set_default_size(GTK_WINDOW(pwDialog), 400, 500);
    g_object_weak_ref(G_OBJECT(pwDialog), DestroyDialog, ptmw);

    GTKRunDialog(pwDialog);
}
