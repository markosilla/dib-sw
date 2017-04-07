#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <testboxd_utilities.h>
#include <libtestbox_log.h>

void alloc_2d_array(char*** memory, int rows, int cols) {
	int i;
	/* In case we fail the returned memory ptr will be initialized */
	*memory = NULL;

	/* defining a temp ptr, otherwise would have to use (*memory) everywhere
	 ptr is used (yuck) */
	char** ptr;

	/* Each row should only contain an unsigned char*, not an unsigned
	 char**, because each row will be an array of unsigned char */
	ptr = malloc(rows * sizeof(char*));

	if (ptr == NULL) {
		TB_LOG(TB_LOG_ERR, "Memory allocation failed!");
	} else {
		/* had an error here.  alloced rows above so iterate through rows
		 not cols here */
		for (i = 0; i < rows; i++) {
			ptr[i] = malloc(cols * sizeof(char));

			if (ptr[i] == NULL) {
				TB_LOG(TB_LOG_ERR, "Memory allocation failed!");
				/* still a problem here, if exiting with error,
				 should free any column mallocs that were
				 successful. */
			}
		}
	}

	/* it worked so return ptr */
	*memory = ptr;
}

/* it also was a bug...would've needed (*ptr) everywhere below */
void free_2d_array(char** ptr, int rows) {
	int i;
	for (i = 0; i < rows; i++) {
		free(ptr[i]);
	}
	free(ptr);
}

/*
 * simple replace function for strings
 * http://www.daniweb.com/software-development/c/code/216517/strings-search-and-replace
 */
char* replace_string(char* source_str, char* search_str, char* replace_str) {
	char *ostr, *nstr = NULL, *pdest = "";
	int length, nlen = 0;
	unsigned int nstr_allocated = 0;
	unsigned int ostr_allocated = 0;

	if (!source_str || !search_str || !replace_str) {
		TB_LOG(TB_LOG_ERR, "Not enough arguments\n");
		return NULL;
	}
	ostr_allocated = sizeof(char) * (strlen(source_str) + 1);
	ostr = malloc(sizeof(char) * (strlen(source_str) + 1));
	if (!ostr) {
		TB_LOG(TB_LOG_ERR, "Insufficient memory available\n");
		return NULL;
	}
	strcpy(ostr, source_str);
	while (pdest) {
		pdest = strstr(ostr, search_str);
		length = (int) (pdest - ostr);
		if (pdest != NULL) {
			ostr[length] = '\0';
			nlen = strlen(ostr) + strlen(replace_str)
					+ strlen(strchr(ostr, 0) + strlen(search_str)) + 1;
			if (!nstr
					|| /* _msize( nstr ) */nstr_allocated
							< sizeof(char) * nlen) {
				nstr_allocated = sizeof(char) * nlen;
				if (nstr)
					free(nstr);
				nstr = malloc(sizeof(char) * nlen);
			}
			if (!nstr) {
				TB_LOG(TB_LOG_ERR, "Insufficient memory available\n");
				if (ostr)
					free(ostr);
				if (pdest)
					free(pdest);
				return NULL;
			}
			strcpy(nstr, ostr);
			strcat(nstr, replace_str);
			strcat(nstr, strchr(ostr, 0) + strlen(search_str));
			if ( /* _msize(ostr) */ostr_allocated
					< sizeof(char) * strlen(nstr) + 1) {
				ostr_allocated = sizeof(char) * strlen(nstr) + 1;
				if (ostr)
					free(ostr);
				ostr = malloc(sizeof(char) * strlen(nstr) + 1);
			}
			if (!ostr) {
				TB_LOG(TB_LOG_ERR, "Insufficient memory available\n");
				if (nstr)
					free(nstr);
				return NULL;
			}
			strcpy(ostr, nstr);
		}
	}
	if (nstr)
		free(nstr);
	return ostr;
}

int time_str_to_sec(char* time_str, int* sec)
{
	char tmp[3];
	int dd = 0, hh = 0, mm = 0, ss = 0;
	int d = -1, h = -1, m = -1, s = -1;
	int i = 0;
	int len = 0;

	/* dd:hh:mm:ss */
	len = strlen(time_str);
	if(strlen(time_str) < 3)
		return -1;

	while(i < len) {
		if(time_str[i] == ':' && d == -1) {
			d = i;
			i++;
		}
		if(time_str[i] == ':' && h == -1) {
			h = i;
			i++;
		}
		if(time_str[i] == ':' && m == -1) {
			m = i;
			i++;
		}
		i++;
	}
	s = len;

	if (d > 0) {
		if (d == 2)
			sprintf(tmp, "%c%c", time_str[d-2], time_str[d-1]);
		else if (d == 1)
			sprintf(tmp, "%c", time_str[d-1]);
		dd = atoi(tmp);
		dd = dd * 86400;
	}
	if ((h-d) > 1) {
		if ((h-d) == 3)
			sprintf(tmp, "%c%c", time_str[h-2], time_str[h-1]);
		else if ((h-d) == 2)
			sprintf(tmp, "%c", time_str[h-1]);
		hh = atoi(tmp);
		hh = hh * 3600;
	}
	if ((m-h) > 1) {
		if ((m-h) == 3)
			sprintf(tmp, "%c%c", time_str[m-2], time_str[m-1]);
		else if ((m-h) == 2)
			sprintf(tmp, "%c", time_str[m-1]);
		mm = atoi(tmp);
		mm = mm * 60;
	}
	if (len > 3) {
		if((s - m) == 3)
			sprintf(tmp, "%c%c", time_str[s-2], time_str[s-1]);
		else if ((s - m) == 2)
			sprintf(tmp, "%c", time_str[s-1]);
		ss = atoi(tmp);
	}
	*sec = (dd + hh + mm + ss);
	//TB_LOG(TB_LOG_ERR, "time_str_to_sec() sec %d\n", *sec);
	return 0;
}
