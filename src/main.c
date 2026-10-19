#include "../include/os.h"
#include "../include/banker.h"
#include "../include/memory.h"
#include "../include/ipc.h"
#include "../include/logger.h"
#include "../include/memory_map.h"
#include "../include/serc_printf.h"
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>

// Global Widgets
static GtkWidget *output_text;
static GtkWidget *drawing_area;
static GtkWidget *gantt_frame;
static GtkWidget *total_mem_label;
static GtkWidget *used_mem_label;
static GtkWidget *free_mem_label;
static GtkWidget *mem_progress;
static GtkEntry *quantum_entry;
static GtkWidget *main_window;
static GtkTextBuffer *output_buffer;

// Define the global function pointer
void (*serc_printf_func)(const char *fmt, ...) = NULL;


static char* get_timestamp(void) {
    static char buf[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

// Helper function to set window icon with multiple fallback methods
static void set_window_icon(GtkWindow *window, const char *icon_filename) {
    GError *error = NULL;
    GdkPixbuf *icon = NULL;

    //  direct file load
    if (g_file_test(icon_filename, G_FILE_TEST_EXISTS)) {
        icon = gdk_pixbuf_new_from_file(icon_filename, &error);
    }

    // alternative names
    if (!icon) {
        const char *alt_names[] = {"icon.ico", "icon.png", "app_icon.png", "logo.ico", NULL};
        for (int i = 0; alt_names[i] != NULL; i++) {
            if (g_file_test(alt_names[i], G_FILE_TEST_EXISTS)) {
                icon = gdk_pixbuf_new_from_file(alt_names[i], &error);
                if (icon) break;
            }
        }
    }

    // Method 3:application directory
    if (!icon) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", g_get_current_dir(), icon_filename);
        if (g_file_test(path, G_FILE_TEST_EXISTS)) {
            icon = gdk_pixbuf_new_from_file(path, &error);
        }
    }

    if (icon) {
        if (GTK_IS_WINDOW(window)) {
            gtk_window_set_icon(window, icon);
            printf("[%s] Window icon set successfully.\n", get_timestamp());
        } else {
            printf("[%s] Warning: Invalid window pointer for icon.\n", get_timestamp());
        }
        g_object_unref(icon);
    } else {
        printf("[%s] Could not set window icon: %s\n", get_timestamp(),
               error ? error->message : "File not found");
        if (error) g_error_free(error);
    }
}

// Custom printf implementation that outputs to GTK
static void gtk_printf_func(const char *fmt, ...) {
    char buffer[8192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Print to console
    fputs(buffer, stdout);
    fflush(stdout);

    // Also append to GTK output
    if (output_buffer) {
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(output_buffer, &end);
        gtk_text_buffer_insert(output_buffer, &end, buffer, -1);
        gtk_text_buffer_get_end_iter(output_buffer, &end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(output_text), &end, 0.0, FALSE, 0.0, 0.0);
    }
}

// Gantt Chart Data
#define MAX_SEGMENTS 256
static sched_segment_t segments[MAX_SEGMENTS];
static int num_seg = 0;

static void update_memory_display(void) {
    int total, used, free;
    get_memory_stats(&total, &used, &free);

    char total_str[64], used_str[64], free_str[64];
    snprintf(total_str, sizeof(total_str), "Total: %d KB", total);
    snprintf(used_str, sizeof(used_str), "Used: %d KB", used);
    snprintf(free_str, sizeof(free_str), "Free: %d KB", free);

    gtk_label_set_text(GTK_LABEL(total_mem_label), total_str);
    gtk_label_set_text(GTK_LABEL(used_mem_label), used_str);
    gtk_label_set_text(GTK_LABEL(free_mem_label), free_str);

    float fraction = (float)used / total;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mem_progress), fraction);

    char percent_str[32];
    snprintf(percent_str, sizeof(percent_str), "Memory Usage: %.1f%%", fraction * 100);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(mem_progress), percent_str);
}

// Process Creation Dialog with Arrival Time
static void show_process_creation_dialog(void) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Create Emergency Task - SERC",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Create Task", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 10);
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);

    // Task Type Selection
    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *type_label = gtk_label_new("Emergency Task Type:");
    gtk_widget_set_size_request(type_label, 180, -1);
    GtkWidget *type_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Ambulance Dispatch (Priority 1)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Fire Service (Priority 2)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Police Coordination (Priority 3)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), 0);
    gtk_box_pack_start(GTK_BOX(type_box), type_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(type_box), type_combo, TRUE, TRUE, 0);

    // Arrival Time
    GtkWidget *arrival_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *arrival_label = gtk_label_new("Arrival Time (ms):");
    gtk_widget_set_size_request(arrival_label, 180, -1);
    GtkWidget *arrival_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(arrival_entry), "0");
    gtk_box_pack_start(GTK_BOX(arrival_box), arrival_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(arrival_box), arrival_entry, TRUE, TRUE, 0);

    // Burst Time
    GtkWidget *burst_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *burst_label = gtk_label_new("Burst Time (ms):");
    gtk_widget_set_size_request(burst_label, 180, -1);
    GtkWidget *burst_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(burst_entry), "30");
    gtk_box_pack_start(GTK_BOX(burst_box), burst_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(burst_box), burst_entry, TRUE, TRUE, 0);

    // Memory Size
    GtkWidget *mem_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *mem_label = gtk_label_new("Memory Required (KB):");
    gtk_widget_set_size_request(mem_label, 180, -1);
    GtkWidget *mem_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(mem_entry), "80");
    gtk_box_pack_start(GTK_BOX(mem_box), mem_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mem_box), mem_entry, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(content), type_box);
    gtk_container_add(GTK_CONTAINER(content), arrival_box);
    gtk_container_add(GTK_CONTAINER(content), burst_box);
    gtk_container_add(GTK_CONTAINER(content), mem_box);

    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        int task_type = gtk_combo_box_get_active(GTK_COMBO_BOX(type_combo));
        const char *name;
        int priority;

        switch(task_type) {
            case 0: name = "Ambulance"; priority = 1; break;
            case 1: name = "Fire Service"; priority = 2; break;
            default: name = "Police"; priority = 3; break;
        }

        int arrival = atoi(gtk_entry_get_text(GTK_ENTRY(arrival_entry)));
        int burst = atoi(gtk_entry_get_text(GTK_ENTRY(burst_entry)));
        int msize = atoi(gtk_entry_get_text(GTK_ENTRY(mem_entry)));

        if (arrival < 0) arrival = 0;
        if (burst <= 0) burst = 30;
        if (msize <= 0) msize = 80;

        int pid = create_process_with_arrival(name, burst, priority, msize, arrival);
        if (pid < 0) {
            printf("[%s] X Failed to create emergency task: Limit reached.\n", get_timestamp());
        } else {
            printf("[%s] OK Emergency Task Created: PID %d (%s) - Arrival=%dms, Burst=%dms, Mem=%dKB\n",
                      get_timestamp(), pid, name, arrival, burst, msize);
        }
        update_memory_display();
    }
    gtk_widget_destroy(dialog);
}

// Gantt Chart Drawing
gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    if (num_seg == 0) return FALSE;

    int total_time = 0;
    for (int i = 0; i < num_seg; i++) {
        total_time += segments[i].duration;
    }

    if (total_time == 0) return FALSE;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    double scale = (double)(width - 80) / total_time;
    int bar_height = 35;
    int y = 25;

    double colors[][3] = {
        {0.9, 0.2, 0.2},
        {0.95, 0.5, 0.1},
        {0.2, 0.4, 0.8},
    };

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.3);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11);
    cairo_move_to(cr, 10, 18);
    cairo_show_text(cr, "Gantt Chart - Execution Timeline");

    int current_x = 35;
    for (int i = 0; i < num_seg; i++) {
        int width_px = (int)(segments[i].duration * scale);
        if (width_px < 2) width_px = 2;

        int color_idx = (segments[i].pid - 1) % 3;
        cairo_set_source_rgb(cr, colors[color_idx][0], colors[color_idx][1], colors[color_idx][2]);
        cairo_rectangle(cr, current_x, y, width_px, bar_height);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 0, 0.3, 0.6);
        cairo_set_line_width(cr, 1.0);
        cairo_rectangle(cr, current_x, y, width_px, bar_height);
        cairo_stroke(cr);

        if (width_px > 25) {
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_set_font_size(cr, 9);
            cairo_move_to(cr, current_x + 5, y + bar_height - 10);
            char label[16];
            snprintf(label, sizeof(label), "P%d", segments[i].pid);
            cairo_show_text(cr, label);
        }

        current_x += width_px;
    }

    cairo_set_source_rgb(cr, 0.2, 0.2, 0.3);
    cairo_set_font_size(cr, 9);

    int time = 0;
    current_x = 35;
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "0");
    cairo_move_to(cr, current_x - 5, y + bar_height + 15);
    cairo_show_text(cr, time_str);

    for (int i = 0; i < num_seg; i++) {
        time += segments[i].duration;
        current_x += (int)(segments[i].duration * scale);
        snprintf(time_str, sizeof(time_str), "%d", time);
        cairo_move_to(cr, current_x - 8, y + bar_height + 15);
        cairo_show_text(cr, time_str);
    }

    return FALSE;
}

// Scheduler Callbacks
void on_fcfs_clicked(GtkWidget *w, gpointer d) {
    printf("\n====================================================================\n");
    printf("[%s] Starting FCFS Emergency Task Scheduler\n", get_timestamp());
    printf("====================================================================\n\n");

    num_seg = run_scheduler(SCHED_FCFS, 0, segments, MAX_SEGMENTS);

    if (num_seg > 0) {
        gtk_widget_show(gantt_frame);
        gtk_widget_queue_draw(drawing_area);
    }

    printf("\n[%s] FCFS Scheduler completed.\n", get_timestamp());
    update_memory_display();
}

void on_rr_clicked(GtkWidget *w, gpointer d) {
    int q = atoi(gtk_entry_get_text(quantum_entry));
    if (q <= 0) q = 10;

    printf("\n====================================================================\n");
    printf("[%s] Starting Round Robin Scheduler (Quantum=%dms)\n", get_timestamp(), q);
    printf("====================================================================\n\n");

    num_seg = run_scheduler(SCHED_ROUNDROBIN, q, segments, MAX_SEGMENTS);

    if (num_seg > 0) {
        gtk_widget_show(gantt_frame);
        gtk_widget_queue_draw(drawing_area);
    }

    printf("\n[%s] Round Robin Scheduler completed.\n", get_timestamp());
    update_memory_display();
}

// Process Management Callbacks
void on_create_clicked(GtkWidget *w, gpointer d) {
    show_process_creation_dialog();
}

void on_list_processes_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] EMERGENCY TASK TABLE\n", get_timestamp());
    list_processes();
}

void on_terminate_clicked(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Terminate Emergency Task",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Terminate", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter PID to terminate");
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        int pid = atoi(gtk_entry_get_text(GTK_ENTRY(entry)));
        terminate_process(pid);
        printf("[%s] Emergency Task PID %d terminated.\n", get_timestamp(), pid);
        update_memory_display();

        if (num_seg > 0) {
            gtk_widget_queue_draw(drawing_area);
        }
    }
    gtk_widget_destroy(dialog);
}

// Memory Callbacks
void on_show_memory_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] MEMORY MAP\n", get_timestamp());
    show_memory();
}

void on_memory_map_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] Opening Graphical Memory Map...\n", get_timestamp());
    show_memory_map_window(GTK_WINDOW(main_window));
}

// Memory Check Callback
void on_check_memory_clicked(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Check Memory Allocation",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Check All Tasks", GTK_RESPONSE_ACCEPT,
        "Check Specific PID", GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 10);
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);

    GtkWidget *label = gtk_label_new("Check memory allocation for emergency tasks:");
    gtk_container_add(GTK_CONTAINER(content), label);

    GtkWidget *pid_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *pid_label = gtk_label_new("PID (optional):");
    GtkWidget *pid_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pid_entry), "Enter PID or leave empty for all tasks");
    gtk_box_pack_start(GTK_BOX(pid_box), pid_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pid_box), pid_entry, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(content), pid_box);

    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT || result == GTK_RESPONSE_OK) {
        const char *pid_text = gtk_entry_get_text(GTK_ENTRY(pid_entry));
        int pid = -1;

        if (pid_text && strlen(pid_text) > 0) {
            pid = atoi(pid_text);
        }

        printf("\n[%s] MEMORY ALLOCATION CHECK\n", get_timestamp());
        check_memory_allocation(pid);
    }

    gtk_widget_destroy(dialog);
}

// Banker's Algorithm Callbacks
void on_init_resources_clicked(GtkWidget *w, gpointer d) {
    int totals[3] = {10, 5, 7};
    init_resources(3, totals);
    printf("\n[%s] Resources Initialised: Channels=10, Vehicles=5, Personnel=7\n", get_timestamp());
}

void on_set_max_clicked(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Set Max Claim - Banker's Algorithm",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Set", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *pid_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(pid_box), gtk_label_new("PID:"), FALSE, FALSE, 0);
    GtkWidget *pid_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(pid_box), pid_entry, TRUE, TRUE, 0);

    GtkWidget *max_entries[3];
    const char *labels[] = {"Max Channels:", "Max Vehicles:", "Max Personnel:"};

    for (int i = 0; i < 3; i++) {
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(box), gtk_label_new(labels[i]), FALSE, FALSE, 0);
        max_entries[i] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(max_entries[i]), "0");
        gtk_box_pack_start(GTK_BOX(box), max_entries[i], TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(content), box);
    }

    gtk_container_add(GTK_CONTAINER(content), pid_box);
    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        int pid = atoi(gtk_entry_get_text(GTK_ENTRY(pid_entry)));
        int max[3];
        for (int i = 0; i < 3; i++) {
            max[i] = atoi(gtk_entry_get_text(GTK_ENTRY(max_entries[i])));
        }
        if (set_max_claim(pid, max) == 0) {
            printf("\n[%s] Max claim set for PID %d\n", get_timestamp(), pid);
        } else {
            printf("\n[%s] Failed: PID %d not found.\n", get_timestamp(), pid);
        }
    }
    gtk_widget_destroy(dialog);
}

void on_request_clicked(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Request Resources - Banker's Algorithm",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Request", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *pid_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(pid_box), gtk_label_new("PID:"), FALSE, FALSE, 0);
    GtkWidget *pid_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(pid_box), pid_entry, TRUE, TRUE, 0);

    GtkWidget *req_entries[3];
    const char *labels[] = {"Request Channels:", "Request Vehicles:", "Request Personnel:"};

    for (int i = 0; i < 3; i++) {
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(box), gtk_label_new(labels[i]), FALSE, FALSE, 0);
        req_entries[i] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(req_entries[i]), "0");
        gtk_box_pack_start(GTK_BOX(box), req_entries[i], TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(content), box);
    }

    gtk_container_add(GTK_CONTAINER(content), pid_box);
    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        int pid = atoi(gtk_entry_get_text(GTK_ENTRY(pid_entry)));
        int req[3];
        for (int i = 0; i < 3; i++) {
            req[i] = atoi(gtk_entry_get_text(GTK_ENTRY(req_entries[i])));
        }
        int res = request_resources(pid, req);
        if (res == 1) {
            printf("\n[%s] Request GRANTED for PID %d\n", get_timestamp(), pid);
        } else if (res == 0) {
            printf("\n[%s] Request DENIED - unsafe state or insufficient resources\n", get_timestamp());
        } else {
            printf("\n[%s] Request DENIED - exceeds max claim\n", get_timestamp());
        }
        show_resources();
    }
    gtk_widget_destroy(dialog);
}

void on_release_clicked(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Release Resources - Banker's Algorithm",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Release", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *pid_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(pid_box), gtk_label_new("PID:"), FALSE, FALSE, 0);
    GtkWidget *pid_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(pid_box), pid_entry, TRUE, TRUE, 0);

    GtkWidget *rel_entries[3];
    const char *labels[] = {"Release Channels:", "Release Vehicles:", "Release Personnel:"};

    for (int i = 0; i < 3; i++) {
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(box), gtk_label_new(labels[i]), FALSE, FALSE, 0);
        rel_entries[i] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(rel_entries[i]), "0");
        gtk_box_pack_start(GTK_BOX(box), rel_entries[i], TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(content), box);
    }

    gtk_container_add(GTK_CONTAINER(content), pid_box);
    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        int pid = atoi(gtk_entry_get_text(GTK_ENTRY(pid_entry)));
        int rel[3];
        for (int i = 0; i < 3; i++) {
            rel[i] = atoi(gtk_entry_get_text(GTK_ENTRY(rel_entries[i])));
        }
        release_resources(pid, rel);
        printf("\n[%s] Resources released by PID %d\n", get_timestamp(), pid);
        show_resources();
    }
    gtk_widget_destroy(dialog);
}

void on_safety_check_clicked(GtkWidget *w, gpointer d) {
    int safe = bankers_safety_check();
    if (safe) {
        char safe_seq[256];
        get_safe_sequence(safe_seq, sizeof(safe_seq));
        printf("\n[%s] SAFETY CHECK: System is in a SAFE state\n", get_timestamp());
        printf("[%s] Safe Sequence: %s\n", get_timestamp(), safe_seq);
    } else {
        printf("\n[%s] WARNING: System is in an UNSAFE state\n", get_timestamp());
    }
}

void on_show_resources_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] RESOURCE ALLOCATION STATUS\n", get_timestamp());
    show_resources();
}

// IPC Callbacks
void on_send_message_clicked(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Send IPC Message",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Send", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *sender_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(sender_box), gtk_label_new("Sender PID:"), FALSE, FALSE, 0);
    GtkWidget *sender_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(sender_box), sender_entry, TRUE, TRUE, 0);

    GtkWidget *receiver_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(receiver_box), gtk_label_new("Receiver PID:"), FALSE, FALSE, 0);
    GtkWidget *receiver_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(receiver_box), receiver_entry, TRUE, TRUE, 0);

    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(type_box), gtk_label_new("Type:"), FALSE, FALSE, 0);
    GtkWidget *type_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(type_entry), "1");
    gtk_box_pack_start(GTK_BOX(type_box), type_entry, TRUE, TRUE, 0);

    GtkWidget *data_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(data_box), gtk_label_new("Message:"), FALSE, FALSE, 0);
    GtkWidget *data_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data_entry), "Enter message");
    gtk_box_pack_start(GTK_BOX(data_box), data_entry, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(content), sender_box);
    gtk_container_add(GTK_CONTAINER(content), receiver_box);
    gtk_container_add(GTK_CONTAINER(content), type_box);
    gtk_container_add(GTK_CONTAINER(content), data_box);
    gtk_widget_show_all(dialog);

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        int sender = atoi(gtk_entry_get_text(GTK_ENTRY(sender_entry)));
        int receiver = atoi(gtk_entry_get_text(GTK_ENTRY(receiver_entry)));
        int type = atoi(gtk_entry_get_text(GTK_ENTRY(type_entry)));
        const char *data = gtk_entry_get_text(GTK_ENTRY(data_entry));

        int msg_id = send_message(sender, receiver, type, data);
        if (msg_id >= 0) {
            printf("\n[%s] IPC Message sent: PID %d -> PID %d (Type=%d, Msg='%s')\n",
                      get_timestamp(), sender, receiver, type, data);
        } else {
            printf("\n[%s] Failed to send message: Queue full.\n", get_timestamp());
        }
    }
    gtk_widget_destroy(dialog);
}

void on_list_messages_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] MESSAGE QUEUE\n", get_timestamp());
    list_messages();
}

void on_list_pipes_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] PIPES\n", get_timestamp());
    list_pipes();
}

void on_list_shared_mem_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] SHARED MEMORY\n", get_timestamp());
    list_shared_memory();
}

// Log Callbacks
void on_show_logs_clicked(GtkWidget *w, gpointer d) {
    printf("\n[%s] EVENT LOGS\n", get_timestamp());
    show_logs();
}

void on_clear_output_clicked(GtkWidget *w, gpointer d) {
    if (output_buffer) {
        gtk_text_buffer_set_text(output_buffer, "", 0);
    }
    printf("[%s] Output cleared.\n", get_timestamp());
}

// Main Function
int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "en_US.UTF-8");

    // Set the global printf function pointer to our GTK version
    serc_printf_func = gtk_printf_func;


    g_setenv("GTK_OVERLAY_SCROLLING", "0", TRUE);

    init_system();
    init_memory(MEMORY_SIZE);
    ipc_init();

    gtk_init(&argc, &argv);

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "SERC Mini-OS - Smart Emergency Response Center");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1400, 900);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Set window icon
    set_window_icon(GTK_WINDOW(main_window), "app_icon.ico");

    // CSS Theme
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css =
        "* { font-family: 'Segoe UI', 'Ubuntu', 'Sans'; font-size: 12px; }"
        "window { background-color: #2c3e50; }"
        "button { background: #e0e0e0; color: #2c3e50; border-radius: 4px; padding: 8px 12px; border: 1px solid #b0b0b0; min-width: 140px; }"
        "button:hover { background: #d0d0d0; }"
        "frame { background-color: #ecf0f1; border-radius: 6px; border: 1px solid #bdc3c7; margin: 3px; }"
        "frame > label { font-weight: bold; font-size: 12px; color: #2c3e50; padding: 6px; background: #bdc3c7; }"
        "entry { background-color: white; border-radius: 3px; padding: 4px; border: 1px solid #b0b0b0; }"
        "textview { background-color: #1e1e1e; border-radius: 4px; }"
        "textview text { background-color: #1e1e1e; color: #00ff00; font-family: 'Monospace'; font-size: 12px; }"
        "scrolledwindow { background-color: #1e1e1e; border-radius: 4px; }"
        "scrolledwindow viewport { background-color: #1e1e1e; }"
        "scrollbar slider { background-color: #888888; border-radius: 5px; min-width: 10px; min-height: 10px; }"
        "scrollbar slider:hover { background-color: #aaaaaa; }"
        "scrollbar { background-color: #333333; }"
        "progressbar trough { background-color: #bdc3c7; border-radius: 3px; min-height: 10px; }"
        "progressbar progress { background: #27ae60; border-radius: 3px; }"
        "label { color: #2c3e50; }";

    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Main horizontal box
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(main_window), main_hbox);

    // LEFT PANEL (WITH SCROLLBAR)
    GtkWidget *left_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left_scrolled),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(left_scrolled, 480, -1);
    gtk_box_pack_start(GTK_BOX(main_hbox), left_scrolled, FALSE, FALSE, 0);

    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(left_panel), 5);
    gtk_container_add(GTK_CONTAINER(left_scrolled), left_panel);

    // Title
    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label),
        "<span size='large' weight='bold' color='#ffffff'>SMART EMERGENCY RESPONSE CENTER</span>\n"
        "<span size='small' color='#c0c0c0'>CS 225 | Dr. Derrick Ntalasha</span>");
    gtk_box_pack_start(GTK_BOX(left_panel), title_label, FALSE, FALSE, 5);

    // Memory Status
    GtkWidget *mem_frame = gtk_frame_new("Memory Status");
    GtkWidget *mem_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(mem_box), 8);

    total_mem_label = gtk_label_new("Total: 1024 KB");
    used_mem_label = gtk_label_new("Used: 0 KB");
    free_mem_label = gtk_label_new("Free: 1024 KB");
    mem_progress = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mem_progress), 0.0);

    gtk_box_pack_start(GTK_BOX(mem_box), total_mem_label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mem_box), used_mem_label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mem_box), free_mem_label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mem_box), mem_progress, FALSE, FALSE, 5);
    gtk_container_add(GTK_CONTAINER(mem_frame), mem_box);
    gtk_box_pack_start(GTK_BOX(left_panel), mem_frame, FALSE, FALSE, 5);

    // Emergency Tasks
    GtkWidget *task_frame = gtk_frame_new("Emergency Tasks");
    GtkWidget *task_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(task_box), 8);

    GtkWidget *create_btn = gtk_button_new_with_label("Create Emergency Task");
    g_signal_connect(create_btn, "clicked", G_CALLBACK(on_create_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(task_box), create_btn, FALSE, FALSE, 3);

    GtkWidget *list_btn = gtk_button_new_with_label("List All Tasks");
    g_signal_connect(list_btn, "clicked", G_CALLBACK(on_list_processes_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(task_box), list_btn, FALSE, FALSE, 3);

    GtkWidget *terminate_btn = gtk_button_new_with_label("Terminate Task");
    g_signal_connect(terminate_btn, "clicked", G_CALLBACK(on_terminate_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(task_box), terminate_btn, FALSE, FALSE, 3);

    gtk_container_add(GTK_CONTAINER(task_frame), task_box);
    gtk_box_pack_start(GTK_BOX(left_panel), task_frame, FALSE, FALSE, 5);

    // CPU Scheduling
    GtkWidget *sched_frame = gtk_frame_new("CPU Scheduling");
    GtkWidget *sched_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(sched_box), 8);

    GtkWidget *fcfs_btn = gtk_button_new_with_label("FCFS Scheduler");
    g_signal_connect(fcfs_btn, "clicked", G_CALLBACK(on_fcfs_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(sched_box), fcfs_btn, FALSE, FALSE, 3);

    GtkWidget *rr_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *rr_btn = gtk_button_new_with_label("Round Robin");
    g_signal_connect(rr_btn, "clicked", G_CALLBACK(on_rr_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(rr_container), rr_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(rr_container), gtk_label_new("Quantum:"), FALSE, FALSE, 0);
    quantum_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_text(quantum_entry, "10");
    gtk_widget_set_size_request(GTK_WIDGET(quantum_entry), 40, -1);
    gtk_box_pack_start(GTK_BOX(rr_container), GTK_WIDGET(quantum_entry), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rr_container), gtk_label_new("ms"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sched_box), rr_container, FALSE, FALSE, 3);
    gtk_container_add(GTK_CONTAINER(sched_frame), sched_box);
    gtk_box_pack_start(GTK_BOX(left_panel), sched_frame, FALSE, FALSE, 5);

    // Memory Management
    GtkWidget *mm_frame = gtk_frame_new("Memory Management");
    GtkWidget *mm_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(mm_box), 8);

    GtkWidget *show_mem_btn = gtk_button_new_with_label("Show Memory Map");
    g_signal_connect(show_mem_btn, "clicked", G_CALLBACK(on_show_memory_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(mm_box), show_mem_btn, FALSE, FALSE, 3);

    GtkWidget *graph_mem_btn = gtk_button_new_with_label("Graphical Memory Map");
    g_signal_connect(graph_mem_btn, "clicked", G_CALLBACK(on_memory_map_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(mm_box), graph_mem_btn, FALSE, FALSE, 3);

    GtkWidget *check_mem_btn = gtk_button_new_with_label("Check Task Memory");
    g_signal_connect(check_mem_btn, "clicked", G_CALLBACK(on_check_memory_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(mm_box), check_mem_btn, FALSE, FALSE, 3);

    gtk_container_add(GTK_CONTAINER(mm_frame), mm_box);
    gtk_box_pack_start(GTK_BOX(left_panel), mm_frame, FALSE, FALSE, 5);

    // Banker's Algorithm
    GtkWidget *bank_frame = gtk_frame_new("Banker's Algorithm (Deadlock Prevention)");
    GtkWidget *bank_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(bank_box), 8);

    GtkWidget *init_res_btn = gtk_button_new_with_label("Init Resources (10,5,7)");
    g_signal_connect(init_res_btn, "clicked", G_CALLBACK(on_init_resources_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bank_box), init_res_btn, FALSE, FALSE, 2);
    GtkWidget *set_max_btn = gtk_button_new_with_label("Set Max Claim");
    g_signal_connect(set_max_btn, "clicked", G_CALLBACK(on_set_max_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bank_box), set_max_btn, FALSE, FALSE, 2);
    GtkWidget *req_btn = gtk_button_new_with_label("Request Resources");
    g_signal_connect(req_btn, "clicked", G_CALLBACK(on_request_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bank_box), req_btn, FALSE, FALSE, 2);
    GtkWidget *release_btn = gtk_button_new_with_label("Release Resources");
    g_signal_connect(release_btn, "clicked", G_CALLBACK(on_release_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bank_box), release_btn, FALSE, FALSE, 2);
    GtkWidget *safe_btn = gtk_button_new_with_label("Safety Check");
    g_signal_connect(safe_btn, "clicked", G_CALLBACK(on_safety_check_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bank_box), safe_btn, FALSE, FALSE, 2);
    GtkWidget *show_res_btn = gtk_button_new_with_label("Show Resource Status");
    g_signal_connect(show_res_btn, "clicked", G_CALLBACK(on_show_resources_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bank_box), show_res_btn, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(bank_frame), bank_box);
    gtk_box_pack_start(GTK_BOX(left_panel), bank_frame, FALSE, FALSE, 5);

    // IPC
    GtkWidget *ipc_frame = gtk_frame_new("Inter-Process Communication");
    GtkWidget *ipc_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(ipc_box), 8);

    GtkWidget *ipc_msg_btn = gtk_button_new_with_label("Send IPC Message");
    g_signal_connect(ipc_msg_btn, "clicked", G_CALLBACK(on_send_message_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(ipc_box), ipc_msg_btn, FALSE, FALSE, 2);
    GtkWidget *list_msg_btn = gtk_button_new_with_label("List Messages");
    g_signal_connect(list_msg_btn, "clicked", G_CALLBACK(on_list_messages_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(ipc_box), list_msg_btn, FALSE, FALSE, 2);
    GtkWidget *list_pipe_btn = gtk_button_new_with_label("List Pipes");
    g_signal_connect(list_pipe_btn, "clicked", G_CALLBACK(on_list_pipes_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(ipc_box), list_pipe_btn, FALSE, FALSE, 2);
    GtkWidget *list_shm_btn = gtk_button_new_with_label("List Shared Memory");
    g_signal_connect(list_shm_btn, "clicked", G_CALLBACK(on_list_shared_mem_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(ipc_box), list_shm_btn, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(ipc_frame), ipc_box);
    gtk_box_pack_start(GTK_BOX(left_panel), ipc_frame, FALSE, FALSE, 5);

    // System Buttons
    GtkWidget *sys_frame = gtk_frame_new("System");
    GtkWidget *sys_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(sys_box), 8);

    GtkWidget *logs_btn = gtk_button_new_with_label("Show Logs");
    g_signal_connect(logs_btn, "clicked", G_CALLBACK(on_show_logs_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(sys_box), logs_btn, TRUE, TRUE, 0);
    GtkWidget *clear_btn = gtk_button_new_with_label("Clear Output");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_output_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(sys_box), clear_btn, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(sys_frame), sys_box);
    gtk_box_pack_start(GTK_BOX(left_panel), sys_frame, FALSE, FALSE, 5);

    // RIGHT PANEL
    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(right_panel), 5);
    gtk_box_pack_start(GTK_BOX(main_hbox), right_panel, TRUE, TRUE, 0);

    // OUTPUT SECTION
    GtkWidget *out_frame = gtk_frame_new("SYSTEM OUTPUT & LOG");
    gtk_box_pack_start(GTK_BOX(right_panel), out_frame, TRUE, TRUE, 0);

    GtkWidget *output_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(output_scrolled),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(output_scrolled), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(out_frame), output_scrolled);

    output_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(output_text), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(output_text), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(output_text), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(output_text), 8);
    gtk_widget_set_size_request(output_text, -1, 400);

    gtk_container_add(GTK_CONTAINER(output_scrolled), output_text);
    output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_text));

    // GANTT CHART SECTION
    gantt_frame = gtk_frame_new("Gantt Chart - Execution Timeline");
    gtk_box_pack_start(GTK_BOX(right_panel), gantt_frame, FALSE, FALSE, 5);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, -1, 120);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), NULL);
    gtk_container_add(GTK_CONTAINER(gantt_frame), drawing_area);
    gtk_widget_hide(gantt_frame);

    // Startup Banner
    printf("============================================================================\n");
    printf("     SMART EMERGENCY RESPONSE CENTER (SERC) - Mini-OS v2.0\n");
    printf("     Copperbelt University | CS 225: Introduction to Operating Systems\n");
    printf("     Instructor: Dr. Derrick Ntalasha\n");
    printf("============================================================================\n\n");

    printf("[%s] System Initialised\n", get_timestamp());
    printf("[%s] Memory Management Active (%d KB)\n", get_timestamp(), MEMORY_SIZE);
    printf("[%s] Process Scheduler Ready (FCFS & Round Robin)\n", get_timestamp());
    printf("[%s] Banker's Algorithm Loaded for Deadlock Prevention\n", get_timestamp());
    printf("[%s] IPC System Initialised (Messages, Pipes, Shared Memory)\n\n", get_timestamp());

    printf("TIPS:\n");
    printf("  - Use 'Create Emergency Task' to add ambulance, fire, or police tasks\n");
    printf("  - Use FCFS or Round Robin to schedule tasks\n");
    printf("  - Banker's Algorithm prevents deadlocks\n");
    printf("  - IPC messages coordinate between emergency teams\n\n");

    log_event("SERC Mini-OS Started");

    gtk_widget_show_all(main_window);
    gtk_main();

    return 0;
}
