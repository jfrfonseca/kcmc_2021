digraph G {
    # GOT IT! KCMC;3 20 1;300 50 100;6752538684;

    outputorder="edgesfirst";
    
    graph [layout=twopi; ranksep=2.0; root=SINK];
    edge  [arrowhead=none];
    node  [style="rounded, filled", fillcolor=white];

    SINK  [style=filled, fillcolor=black, shape=octagon, fontcolor=white, pos="150,150"]
    POI_0 [style=filled, fillcolor=green, shape=circle, pos="68,1"]
    POI_1 [style=filled, fillcolor=green, shape=circle, pos="75,107"]
    POI_2 [style=filled, fillcolor=green, shape=circle, pos="32,187"]
    i_0   [pos="113,84"]
    i_1   [pos="170,75"]
    i_2   [pos="125,149"]
    i_3   [pos="30,10"]
    i_4   [pos="49,234"]
    i_5   [pos="151,225"]
    i_6   [pos="250,279"]
    i_7   [pos="234,137"]
    i_8   [pos="79,33"]
    i_9   [pos="72,228"]
    i_10  [pos="280,296"]
    i_11  [pos="56,146"]
    i_12  [pos="170,251"]
    i_13  [pos="46,167"]
    i_14  [pos="57,194"]
    i_15  [pos="65,42"]
    i_16  [pos="44,111"]
    i_17  [pos="296,158"]
    i_18  [pos="67,249"]
    i_19  [pos="15,26"]

    subgraph cluster_best_reuse {
        node [style=filled, fillcolor=red];
        i0; i8; i11; i15; i19;
    }
    
    ## POI-Sensor connections
    POI_0 -> i0;
    POI_0 -> i8;
    POI_0 -> i15;
    POI_0 -> i16;

    POI_1 -> i0;
    POI_1 -> i8;
    POI_1 -> i15;
    POI_1 -> i16;

    POI_2 -> i3;
    POI_2 -> i8;
    POI_2 -> i15;
    POI_2 -> i19;

    ## Sensor-Sink connections
    i0 -> SINK;
    i1 -> SINK;
    i2 -> SINK;
    i5 -> SINK;
    i7 -> SINK;
    i11 -> SINK;

    ## Sensor-sensor connections
    i0 -> i2;
    i0 -> i3;
    i0 -> i4;
    i0 -> i5;
    i0 -> i8;
    i0 -> i9;
    i0 -> i11;
    i0 -> i13;
    i0 -> i14;
    i0 -> i15;
    i0 -> i16;
    i0 -> i18;
    i1 -> i2;
    i1 -> i5;
    i1 -> i8;
    i1 -> i9;
    i1 -> i12;
    i2 -> i5;
    i2 -> i8;
    i2 -> i9;
    i2 -> i12;
    i3 -> i4;
    i3 -> i8;
    i3 -> i9;
    i3 -> i11;
    i3 -> i13;
    i3 -> i14;
    i3 -> i15;
    i3 -> i16;
    i3 -> i18;
    i3 -> i19;
    i5 -> i7;
    i5 -> i12;
    i6 -> i7;
    i6 -> i10;
    i6 -> i17;
    i7 -> i12;
    i8 -> i9;
    i8 -> i11;
    i8 -> i13;
    i8 -> i14;
    i8 -> i15;
    i8 -> i16;
    i8 -> i18;
    i8 -> i19;
    i10 -> i17;
    i11 -> i14;
    i11 -> i15;
    i11 -> i18;
    i15 -> i16;
    i15 -> i18;
    i15 -> i19;
    i16 -> i18;
}