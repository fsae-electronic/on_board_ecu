#ifndef PAGES_H
#define PAGES_H

typedef enum
{
    PAGE_RACE = 0,
    PAGE_TELEMETRY,
    PAGE_DEBUG,
    PAGE_GRAPH
} ui_page_t;

typedef enum
{
    GRAPH_NONE = 0,
    GRAPH_M1_VDC,
    GRAPH_M1_IDC,
    GRAPH_M1_IAC,
    GRAPH_M1_T,
    GRAPH_M2_VDC,
    GRAPH_M2_IDC,
    GRAPH_M2_IAC,
    GRAPH_M2_T,
    GRAPH_TPS,
    GRAPH_STEER,
    GRAPH_FRONT_BRK,
    GRAPH_REAR_BRK,
    GRAPH_FL_SPD,
    GRAPH_FR_SPD,
    GRAPH_RL_SPD,
    GRAPH_RR_SPD
} graph_metric_t;

extern ui_page_t current_page;
extern graph_metric_t current_graph;

#endif
