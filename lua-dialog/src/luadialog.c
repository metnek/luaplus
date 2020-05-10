#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <kenv.h>
#include <dialog.h>
#include <dlg_keys.h>


static void l_pushtablestring(lua_State* L, char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static void l_pushtablenumber(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}

static void l_pushtableinteger(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

static void l_pushtablenil(lua_State* L, char* key)
{
    lua_pushstring(L, key);
    lua_pushnil(L);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_string(lua_State* L, int key , char* value)
{
    lua_pushinteger(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_number(lua_State* L, int key , int value)
{
    lua_pushinteger(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_integer(lua_State* L, int key , int value)
{
    lua_pushinteger(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_nil(lua_State* L, int key)
{
    lua_pushinteger(L, key);
    lua_pushnil(L);
    lua_settable(L, -3);
}

typedef struct luadialog_vars {
	DIALOG_VARS *dialog_vars;
	char *cprompt;
	char *title;
	char *states;
	int height;
	int width;
	int list_height;
} luadialog_vars_t;

static void
luadialog_fill_vars(lua_State *L, luadialog_vars_t *ldialog_vars)
{
    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
    	if (strcmp(lua_tostring(L, -2), "title") == 0) {
    		ldialog_vars->title = (char *)lua_tostring(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "ok_b") == 0) {
    		ldialog_vars->dialog_vars->ok_label = (char *)lua_tostring(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "cancel_b") == 0) {
    		ldialog_vars->dialog_vars->cancel_label = (char *)lua_tostring(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "extra_b") == 0) {
    		ldialog_vars->dialog_vars->extra_button = TRUE;
    		if (lua_isstring(L, -1))
	    		ldialog_vars->dialog_vars->extra_label = (char *)lua_tostring(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "help_b") == 0) {
    		ldialog_vars->dialog_vars->help_button = TRUE;
    		if (lua_isstring(L, -1))
	    		ldialog_vars->dialog_vars->help_label = (char *)lua_tostring(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "prompt") == 0) {
    		ldialog_vars->cprompt = (char *)lua_tostring(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "height") == 0) {
    		ldialog_vars->height = lua_tonumber(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "width") == 0) {
    		ldialog_vars->width = lua_tonumber(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "list_h") == 0) {
    		ldialog_vars->list_height = lua_tonumber(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "backtitle") == 0) {
    		ldialog_vars->dialog_vars->backtitle = (char *)lua_tostring(L, -1);
    	} else if (strcmp(lua_tostring(L, -2), "help") == 0) {
    		ldialog_vars->dialog_vars->item_help = TRUE;
    	} else if (strcmp(lua_tostring(L, -2), "states") == 0) {
    		ldialog_vars->states = (char *)lua_tostring(L, -1);
    	}
        lua_pop(L, 1);
    }
}

static int libdialog_checklist(lua_State *L)
{
	luadialog_vars_t ldialog_vars;
	ldialog_vars.height = 35;
	ldialog_vars.width = 70;
	ldialog_vars.list_height = 10;
	ldialog_vars.cprompt = "";
	ldialog_vars.title = "";
	ldialog_vars.states = NULL;
	ldialog_vars.dialog_vars = &dialog_vars;

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));
    dialog_vars.item_help = FALSE;

	dialog_vars.backtitle = __DECONST(char *, "");
	if (lua_gettop(L) == 2) {
		luadialog_fill_vars(L, &ldialog_vars);
		lua_settop(L, 1);
	}

	int item_no = lua_rawlen(L, 1);

    dialog_state.output = stderr;
    dialog_state.input = stdin;
    dialog_vars.default_button = -1;

    init_dialog(stdin, stderr);
	dlg_clear();
	dlg_put_backtitle();

    int result;
    int i, j;
    DIALOG_LISTITEM *listitems;
    bool show_status = FALSE;
    int current = 0;

    dialog_vars.separate_output = TRUE;
    dialog_vars.output_separator = " ";
	int flag = FLAG_CHECK;
    int separate_output = ((flag == FLAG_CHECK)
			    && (dialog_vars.separate_output));

    listitems = dlg_calloc(DIALOG_LISTITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_checklist");

    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
    	j = -1;
	    lua_pushnil(L);
	    while(lua_next(L, -2) != 0) {
	    	if (strcmp("num", lua_tostring(L, -2)) == 0) {
	    		j = lua_tonumber(L, -1);
	    	}
	    	lua_pop(L, 1);
	    }
	    if (j != -1) {
	    	j--;
		    lua_pushnil(L);
		    while(lua_next(L, -2) != 0) {
		    	if (strcmp("name", lua_tostring(L, -2)) == 0) {
					listitems[j].name = (char *)lua_tostring(L, -1);
		    	} else if (strcmp("text", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text = (char *)lua_tostring(L, -1);
		    	} else if (strcmp("state", lua_tostring(L, -2)) == 0) {
		    		listitems[j].state = lua_tonumber(L, -1);
		    	} else if (strcmp("help", lua_tostring(L, -2)) == 0) {
		    		listitems[j].help = (char *)lua_tostring(L, -1);
		    	}
		    	lua_pop(L, 1);
		    }
		}
        lua_pop(L, 1);
    }

    dlg_align_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);
    result = dlg_checklist(ldialog_vars.title,
			   ldialog_vars.cprompt,
			   ldialog_vars.height,
			   ldialog_vars.width,
			   ldialog_vars.list_height,
			   item_no,
			   listitems,
			   ldialog_vars.states,
			   flag,
			   &current);
    j = 0;
    switch (result) {
    case DLG_EXIT_OK:		/* FALLTHRU */
    case DLG_EXIT_EXTRA:
    case DLG_EXIT_HELP:
	show_status = TRUE;
	break;
    }
    if (show_status) {
    	j = 1;
		lua_newtable(L);
		l_pushtableinteger(L, "key", result);
		for (i = 0; i < item_no; i++) {
			lua_pushinteger(L, i + 1);
			lua_newtable(L);
			l_pushtableinteger(L, "state", listitems[i].state);
			l_pushtableinteger(L, "num", i + 1);
			l_pushtablestring(L, "name", listitems[i].name);
			l_pushtablestring(L, "text", listitems[i].text);
			if (dialog_vars.item_help == TRUE)
				l_pushtablestring(L, "help", listitems[i].help);
		    lua_settable(L, -3);
		}
		dlg_add_last_key(-1);
    }

    dlg_free_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);
    free(listitems);

    dlg_killall_bg(&result);
	(void) refresh();

	end_dialog();
	if (!j)
		lua_pushnil(L);
    return 1;
}

static int libdialog_radiolist(lua_State *L)
{
	luadialog_vars_t ldialog_vars;
	ldialog_vars.height = 35;
	ldialog_vars.width = 70;
	ldialog_vars.list_height = 10;
	ldialog_vars.cprompt = "";
	ldialog_vars.title = "";
	ldialog_vars.states = NULL;
	ldialog_vars.dialog_vars = &dialog_vars;

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));
    dialog_vars.item_help = FALSE;

	dialog_vars.backtitle = __DECONST(char *, "");
	if (lua_gettop(L) == 2) {
		luadialog_fill_vars(L, &ldialog_vars);
		lua_settop(L, 1);
	}

	int item_no = lua_rawlen(L, 1);

    dialog_state.output = stderr;
    dialog_state.input = stdin;
    dialog_vars.default_button = -1;

    init_dialog(stdin, stderr);
	dlg_clear();
	dlg_put_backtitle();

    int result;
    int i, j;
    DIALOG_LISTITEM *listitems;
    bool show_status = FALSE;
    int current = 0;

    dialog_vars.separate_output = TRUE;
    dialog_vars.output_separator = " ";
	int flag = FLAG_RADIO;
    int separate_output = ((flag == FLAG_CHECK)
			    && (dialog_vars.separate_output));

    listitems = dlg_calloc(DIALOG_LISTITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_checklist");

    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
    	j = -1;
	    lua_pushnil(L);
	    while(lua_next(L, -2) != 0) {
	    	if (strcmp("num", lua_tostring(L, -2)) == 0) {
	    		j = lua_tonumber(L, -1);
	    	}
	    	lua_pop(L, 1);
	    }
	    if (j != -1) {
	    	j--;
		    lua_pushnil(L);
		    while(lua_next(L, -2) != 0) {
		    	if (strcmp("name", lua_tostring(L, -2)) == 0) {
					listitems[j].name = (char *)lua_tostring(L, -1);
		    	} else if (strcmp("text", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text = (char *)lua_tostring(L, -1);
		    	} else if (strcmp("state", lua_tostring(L, -2)) == 0) {
		    		listitems[j].state = lua_tonumber(L, -1);
		    	} else if (strcmp("help", lua_tostring(L, -2)) == 0) {
		    		listitems[j].help = (char *)lua_tostring(L, -1);
		    	}
		    	lua_pop(L, 1);
		    }
		}
        lua_pop(L, 1);
    }

    dlg_align_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);
    result = dlg_checklist(ldialog_vars.title,
			   ldialog_vars.cprompt,
			   ldialog_vars.height,
			   ldialog_vars.width,
			   ldialog_vars.list_height,
			   item_no,
			   listitems,
			   ldialog_vars.states,
			   flag,
			   &current);
    j = 0;
    switch (result) {
    case DLG_EXIT_OK:		/* FALLTHRU */
    case DLG_EXIT_EXTRA:
    case DLG_EXIT_HELP:
	show_status = TRUE;
	break;
    }
    if (show_status) {
    	j = 1;
		lua_newtable(L);
		l_pushtableinteger(L, "key", result);
		for (i = 0; i < item_no; i++) {
			lua_pushinteger(L, i + 1);
			lua_newtable(L);
			l_pushtableinteger(L, "state", listitems[i].state);
			l_pushtableinteger(L, "num", i + 1);
			l_pushtablestring(L, "name", listitems[i].name);
			l_pushtablestring(L, "text", listitems[i].text);
			if (dialog_vars.item_help == TRUE)
				l_pushtablestring(L, "help", listitems[i].help);
		    lua_settable(L, -3);
		}
		dlg_add_last_key(-1);
    }

    dlg_free_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);
    free(listitems);

    dlg_killall_bg(&result);
	(void) refresh();

	end_dialog();
	if (!j)
		lua_pushnil(L);
    return 1;
}


static int libdialog_yesno(lua_State *L)
{
	luadialog_vars_t ldialog_vars;
	ldialog_vars.height = 35;
	ldialog_vars.width = 70;
	ldialog_vars.list_height = 10;
	ldialog_vars.cprompt = "";
	ldialog_vars.title = "";
	ldialog_vars.states = NULL;
	ldialog_vars.dialog_vars = &dialog_vars;

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));
    dialog_vars.item_help = FALSE;

	dialog_vars.backtitle = __DECONST(char *, "");
	luadialog_fill_vars(L, &ldialog_vars);
    dialog_state.output = stderr;
    dialog_state.input = stdin;
    dialog_vars.default_button = -1;

    init_dialog(stdin, stderr);
	dlg_clear();
	dlg_put_backtitle();

    int result;
    result = dialog_yesno(ldialog_vars.title, ldialog_vars.cprompt, ldialog_vars.height, ldialog_vars.width);

	end_dialog();

	lua_pushboolean(L, !result);
    return 1;
}

static int libdialog_form(lua_State *L)
{
	luadialog_vars_t ldialog_vars;
	ldialog_vars.height = 35;
	ldialog_vars.width = 70;
	ldialog_vars.list_height = 10;
	ldialog_vars.cprompt = "";
	ldialog_vars.title = "";
	ldialog_vars.states = NULL;
	ldialog_vars.dialog_vars = &dialog_vars;

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));
    dialog_vars.item_help = FALSE;

	dialog_vars.backtitle = __DECONST(char *, "");
	if (lua_gettop(L) == 2) {
		luadialog_fill_vars(L, &ldialog_vars);
		lua_settop(L, 1);
	}
	int item_no = lua_rawlen(L, 1);

    dialog_state.output = stderr;
    dialog_state.input = stdin;
    dialog_vars.default_button = -1;

    init_dialog(stdin, stderr);
	dlg_clear();
	dlg_put_backtitle();

    int result;
    int choice;
    int i, j;
    DIALOG_FORMITEM *listitems;
    bool show_status = FALSE;
    DIALOG_VARS save_vars;
    dlg_save_vars(&save_vars);

    dialog_vars.separate_output = TRUE;

    listitems = dlg_calloc(DIALOG_FORMITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_form");

    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
    	j = -1;
	    lua_pushnil(L);
	    while(lua_next(L, -2) != 0) {
	    	if (strcmp("num", lua_tostring(L, -2)) == 0) {
	    		j = lua_tonumber(L, -1);
	    	}
	    	lua_pop(L, 1);
	    }
	    if (j != -1) {
	    	j--;
			listitems[j].type = dialog_vars.formitem_type;
    		listitems[j].help = dlg_strempty();
		    lua_pushnil(L);
		    while(lua_next(L, -2) != 0) {
		    	if (strcmp("name", lua_tostring(L, -2)) == 0) {
					listitems[j].name = (char *)lua_tostring(L, -1);
					listitems[j].name_len = strlen(listitems[j].name);
		    	} else if (strcmp("name_y", lua_tostring(L, -2)) == 0) {
		    		listitems[j].name_y = lua_tonumber(L, -1) > 0 ? lua_tonumber(L, -1) : 0;
		    	} else if (strcmp("name_x", lua_tostring(L, -2)) == 0) {
		    		listitems[j].name_x = lua_tonumber(L, -1) > 0 ? lua_tonumber(L, -1) : 0;
		    	} else if (strcmp("text_y", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text_y = lua_tonumber(L, -1) > 0 ? lua_tonumber(L, -1) : 0;
		    	} else if (strcmp("text", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text = (char *)lua_tostring(L, -1);
					listitems[j].text_len = strlen(listitems[j].text);
		    	} else if (strcmp("text_x", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text_x = lua_tonumber(L, -1) > 0 ? lua_tonumber(L, -1) : 0;
		    	} else if (strcmp("flen", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text_flen = lua_tonumber(L, -1);
		    	} else if (strcmp("ilen", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text_ilen = lua_tonumber(L, -1);
		    	} else if (strcmp("help", lua_tostring(L, -2)) == 0) {
					if (ldialog_vars.dialog_vars->item_help == TRUE)
			    		listitems[j].help = (char *)lua_tostring(L, -1);
		    	}
		    	lua_pop(L, 1);
		    }
		}
        lua_pop(L, 1);
    }

    result = dlg_form(ldialog_vars.title,
			   ldialog_vars.cprompt,
			   ldialog_vars.height,
			   ldialog_vars.width,
			   ldialog_vars.list_height,
			   item_no,
			   listitems,
			   &choice);
    j = 0;
    switch (result) {
    case DLG_EXIT_OK:		/* FALLTHRU */
    case DLG_EXIT_EXTRA:
    case DLG_EXIT_HELP:
	show_status = TRUE;
	break;
    }
    if (show_status) {
    	j = 1;
		lua_newtable(L);
		l_pushtableinteger(L, "key", result);
		for (i = 0; i < item_no; i++) {
			lua_pushinteger(L, i + 1);
			lua_newtable(L);
			l_pushtablestring(L, "name", listitems[i].name);
			l_pushtablestring(L, "text", listitems[i].text);

			l_pushtableinteger(L, "name_y", listitems[i].name_y);
			l_pushtableinteger(L, "name_x", listitems[i].name_x);
			l_pushtableinteger(L, "text_y", listitems[i].text_y);
			l_pushtableinteger(L, "text_x", listitems[i].text_x);
			l_pushtableinteger(L, "flen", listitems[i].text_flen);
			l_pushtableinteger(L, "ilen", listitems[i].text_ilen);
			l_pushtableinteger(L, "num", i + 1);

			if (ldialog_vars.dialog_vars->item_help == TRUE)
				l_pushtablestring(L, "help", listitems[i].help);
		    lua_settable(L, -3);
		}
		dlg_add_last_key(-1);
    }

    dlg_free_formitems(listitems);

    dlg_restore_vars(&save_vars);
    dlg_killall_bg(&result);
	(void) refresh();

	end_dialog();
	if (!j)
		lua_pushnil(L);
    return 1;
}

static int libdialog_msgbox(lua_State *L)
{
	luadialog_vars_t ldialog_vars;
	ldialog_vars.height = 35;
	ldialog_vars.width = 70;
	ldialog_vars.list_height = 10;
	ldialog_vars.cprompt = "";
	ldialog_vars.title = "";
	ldialog_vars.states = NULL;
	ldialog_vars.dialog_vars = &dialog_vars;

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));
    dialog_vars.item_help = FALSE;

	dialog_vars.backtitle = __DECONST(char *, "");
	luadialog_fill_vars(L, &ldialog_vars);
    dialog_state.output = stderr;
    dialog_state.input = stdin;
    dialog_vars.default_button = -1;

    init_dialog(stdin, stderr);
	dlg_clear();
	dlg_put_backtitle();

    int result;
    result = dialog_msgbox(ldialog_vars.title, ldialog_vars.cprompt, ldialog_vars.height, ldialog_vars.width, 1);

	end_dialog();

	lua_pushnil(L);
    return 1;
}

static int libdialog_menu(lua_State *L)
{
	luadialog_vars_t ldialog_vars;
	ldialog_vars.height = 35;
	ldialog_vars.width = 70;
	ldialog_vars.list_height = 10;
	ldialog_vars.cprompt = "";
	ldialog_vars.title = "";
	ldialog_vars.states = NULL;
	ldialog_vars.dialog_vars = &dialog_vars;

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));
    dialog_vars.item_help = FALSE;

	dialog_vars.backtitle = __DECONST(char *, "");
	if (lua_gettop(L) == 2) {
		luadialog_fill_vars(L, &ldialog_vars);
		lua_settop(L, 1);
	}
	int item_no = lua_rawlen(L, 1);

    dialog_state.output = stderr;
    dialog_state.input = stdin;
    dialog_vars.default_button = -1;

    init_dialog(stdin, stderr);
	dlg_clear();
	dlg_put_backtitle();

    int result;
    int choice;
    int i, j;
    bool show_status = FALSE;
    DIALOG_LISTITEM *listitems;

    listitems = dlg_calloc(DIALOG_LISTITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_menu");

    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
    	j = -1;
	    lua_pushnil(L);
	    while(lua_next(L, -2) != 0) {
	    	if (strcmp("num", lua_tostring(L, -2)) == 0) {
	    		j = lua_tonumber(L, -1);
	    	}
	    	lua_pop(L, 1);
	    }
	    if (j != -1) {
	    	j--;
    		listitems[j].help = dlg_strempty();
		    lua_pushnil(L);
		    while(lua_next(L, -2) != 0) {
		    	if (strcmp("name", lua_tostring(L, -2)) == 0) {
					listitems[j].name = (char *)lua_tostring(L, -1);
		    	} else if (strcmp("text", lua_tostring(L, -2)) == 0) {
		    		listitems[j].text = (char *)lua_tostring(L, -1);
		    	} else if (strcmp("help", lua_tostring(L, -2)) == 0) {
					if (ldialog_vars.dialog_vars->item_help == TRUE)
			    		listitems[j].help = (char *)lua_tostring(L, -1);
		    	}
		    	lua_pop(L, 1);
		    }
		}
        lua_pop(L, 1);
    }
    dlg_align_columns(&listitems[0].text, sizeof(DIALOG_LISTITEM), item_no);

    result = dlg_menu(ldialog_vars.title,
		      ldialog_vars.cprompt,
		      ldialog_vars.height,
		      ldialog_vars.width,
		      ldialog_vars.list_height,
		      item_no,
		      listitems,
		      &choice,
		      dlg_dummy_menutext);
    printf("%d\n", choice);
    switch (result) {
    case DLG_EXIT_OK:		/* FALLTHRU */
    case DLG_EXIT_EXTRA:
    case DLG_EXIT_HELP:
	show_status = TRUE;
	break;
    }
    if (show_status) {
    	j = 1;
		lua_newtable(L);
		l_pushtableinteger(L, "key", result);
		lua_pushinteger(L, 1);
		lua_newtable(L);
		l_pushtablestring(L, "name", listitems[choice].name);
		l_pushtablestring(L, "text", listitems[choice].text);
		l_pushtableinteger(L, "num", choice);
		if (ldialog_vars.dialog_vars->item_help == TRUE)
			l_pushtablestring(L, "help", listitems[choice].help);
	    lua_settable(L, -3);
		dlg_add_last_key(-1);
    }
    dlg_free_columns(&listitems[0].text, sizeof(DIALOG_LISTITEM), item_no);
    free(listitems);


    dlg_killall_bg(&result);
	(void) refresh();

	end_dialog();

	if (!j)
		lua_pushnil(L);
    return 1;
}

static int libdialog_textbox(lua_State *L)
{
	luadialog_vars_t ldialog_vars;
	ldialog_vars.height = 35;
	ldialog_vars.width = 70;
	ldialog_vars.list_height = 10;
	ldialog_vars.cprompt = "";
	ldialog_vars.title = "";
	ldialog_vars.states = NULL;
	ldialog_vars.dialog_vars = &dialog_vars;

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));
    dialog_vars.item_help = FALSE;

	dialog_vars.backtitle = __DECONST(char *, "");
	if (lua_gettop(L) == 2) {
		luadialog_fill_vars(L, &ldialog_vars);
	}

    dialog_state.output = stderr;
    dialog_state.input = stdin;
    dialog_vars.default_button = -1;

    init_dialog(stdin, stderr);
	dlg_clear();
	dlg_put_backtitle();

    int result;
    char *file = (char *)luaL_optstring(L, 1, NULL);
    if (file != NULL)
	    result = dialog_textbox(ldialog_vars.title, file, ldialog_vars.height, ldialog_vars.width);

	end_dialog();

	lua_pushnil(L);
    return 1;
}

static int libdialog_test(lua_State *L)
{
	lua_pushnil(L);
    return 1;
}


static const luaL_Reg dialog[] = {
    {"checklist", libdialog_checklist},
    {"yesno", libdialog_yesno},
    {"form", libdialog_form},
    {"msgbox", libdialog_msgbox},
    {"menu", libdialog_menu},
    {"textbox", libdialog_textbox},
    {"radiolist", libdialog_radiolist},
    {"test", libdialog_test}, 
    {NULL, NULL}
};


#ifdef STATIC
LUAMOD_API int luaopen_libluadialog(lua_State *L)
#else
LUALIB_API int luaopen_libluadialog(lua_State *L)
#endif
{
    luaL_newlib(L, dialog);
    return 1;
}
