digraph G {
    # GOT IT! KCMC;3 20 1;300 50 100;222551069;

    outputorder="edgesfirst";
    
    graph [layout=twopi; ranksep=2.0; root=SINK];
    edge  [arrowhead=none];
    node  [style="rounded, filled", fillcolor=white];
    
    SINK  [style=filled, fillcolor=black, shape=octagon, fontcolor=white];
    
    POI_0 [style=filled, fillcolor=green, shape=circle];
    POI_1 [style=filled, fillcolor=green, shape=circle];
    POI_2 [style=filled, fillcolor=green, shape=circle];
    
    subgraph cluster_best_reuse {
        node [style=filled, fillcolor=red];
        i6; i7; i12; i18; i19;
    }
    
    ## POI-Sensor connections
    POI_0 -> i0;
    POI_0 -> i5;
    POI_0 -> i7;
    POI_0 -> i12;
    POI_0 -> i19;

    POI_1 -> i0;
    POI_1 -> i5;
    POI_1 -> i7;
    POI_1 -> i9;
    POI_1 -> i12;
    POI_1 -> i18;
    POI_1 -> i19;

    POI_2 -> i2;
    POI_2 -> i4;
    POI_2 -> i5;
    POI_2 -> i6;
    POI_2 -> i7;
    POI_2 -> i18;

    ## Sensor-Sink connections
    i2 -> SINK;
    i3 -> SINK;
    i4 -> SINK;
    i6 -> SINK;
    i10 -> SINK;
    i13 -> SINK;
    i15 -> SINK;
    i18 -> SINK;

    ## Sensor-sensor connections
    i0 -> i1;
    i0 -> i2;
    i0 -> i5;
    i0 -> i6;
    i0 -> i7;
    i0 -> i9;
    i0 -> i12;
    i0 -> i13;
    i0 -> i16;
    i0 -> i18;
    i0 -> i19;
    i2 -> i4;
    i2 -> i5;
    i2 -> i6;
    i2 -> i7;
    i2 -> i9;
    i2 -> i12;
    i2 -> i13;
    i2 -> i16;
    i2 -> i18;
    i2 -> i19;
    i3 -> i4;
    i3 -> i8;
    i3 -> i10;
    i3 -> i11;
    i3 -> i14;
    i3 -> i15;
    i4 -> i5;
    i4 -> i6;
    i4 -> i7;
    i4 -> i8;
    i4 -> i10;
    i4 -> i13;
    i4 -> i15;
    i4 -> i16;
    i4 -> i18;
    i4 -> i19;
    i5 -> i6;
    i5 -> i7;
    i5 -> i9;
    i5 -> i12;
    i5 -> i13;
    i5 -> i16;
    i5 -> i18;
    i5 -> i19;
    i6 -> i7;
    i6 -> i8;
    i6 -> i9;
    i6 -> i10;
    i6 -> i13;
    i6 -> i16;
    i6 -> i18;
    i6 -> i19;
    i7 -> i9;
    i7 -> i12;
    i7 -> i13;
    i7 -> i16;
    i7 -> i18;
    i7 -> i19;
    i8 -> i13;
    i8 -> i16;
    i8 -> i18;
    i8 -> i19;
    i9 -> i12;
    i9 -> i13;
    i9 -> i16;
    i9 -> i18;
    i9 -> i19;
    i10 -> i11;
    i10 -> i14;
    i10 -> i15;
    i11 -> i14;
    i11 -> i15;
    i11 -> i17;
    i12 -> i13;
    i12 -> i16;
    i12 -> i18;
    i12 -> i19;
    i13 -> i15;
    i18 -> i19;
}