#include "apotheke-highlight-buffer.h"

void
apotheke_highlight_buffer_diff (GtkTextBuffer *buffer, GtkTextIter *start)
{
	GtkTextIter *end;
	GtkTextIter *word_end;
	gchar *c;

	if (!gtk_text_iter_starts_line (start))
		gtk_text_iter_set_line_offset (start, 0);


	while (!gtk_text_iter_is_end (start)) {
		end = gtk_text_iter_copy (start);
		if (!gtk_text_iter_forward_to_line_end (end))
			break;

		word_end = gtk_text_iter_copy (start);
		gtk_text_iter_forward_char (word_end);
		
		c = gtk_text_iter_get_text (start, word_end);
		if (g_ascii_strcasecmp ("+", c) == 0) {
			gtk_text_buffer_apply_tag_by_name (buffer,
							   "diff-added",
							   start, end);
		} 
		else if (g_ascii_strcasecmp ("-", c) == 0) {
			gtk_text_buffer_apply_tag_by_name (buffer,
							   "diff-removed",
							   start, end);
		}

		gtk_text_iter_forward_line (start);
	}
}
