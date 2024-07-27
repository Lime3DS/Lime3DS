// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 430 core
precision mediump float;

layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec4 frag_color;
layout(binding = 0) uniform sampler2D tex;

#define src(x, y) texture(tex, coord + vec2(x, y) * 1.0 / source_size)

float luma(vec4 col){
    return dot(col.rgb, vec3(0.2126, 0.7152, 0.0722)) * (1.0 - col.a);
}

bool same(vec4 B, vec4 A0){
    return all(equal(B, A0));
}

bool notsame(vec4 B, vec4 A0){
    return any(notEqual(B, A0));
}

bool all_eq2(vec4 B, vec4 A0, vec4 A1) {
    return (same(B,A0) && same(B,A1));
}

bool all_eq3(vec4 B, vec4 A0, vec4 A1, vec4 A2) {
    return (same(B,A0) && same(B,A1) && same(B,A2));
}

bool all_eq4(vec4 B, vec4 A0, vec4 A1, vec4 A2, vec4 A3) {
    return (same(B,A0) && same(B,A1) && same(B,A2) && same(B,A3));
}

bool any_eq3(vec4 B, vec4 A0, vec4 A1, vec4 A2) {
    return (same(B,A0) || same(B,A1) || same(B,A2));
}

bool none_eq2(vec4 B, vec4 A0, vec4 A1) {
    return (notsame(B,A0) && notsame(B,A1));
}

bool none_eq4(vec4 B, vec4 A0, vec4 A1, vec4 A2, vec4 A3) {
    return (notsame(B,A0) && notsame(B,A1) && notsame(B,A2) && notsame(B,A3));
}

void main()
{
    vec2 source_size = vec2(textureSize(tex, 0));
    vec2 pos = fract(tex_coord * source_size) - vec2(0.5, 0.5);
    vec2 coord = tex_coord - pos / source_size;

    vec4 E = src(0.0,0.0);

    vec4 A = src(-1.0,-1.0);
    vec4 B = src(0.0,-1.0);
    vec4 C = src(1.0,-1.0);

    vec4 D = src(-1.0,0.0);
    vec4 F = src(1.0,0.0);

    vec4 G = src(-1.0,1.0);
    vec4 H = src(0.0,1.0);
    vec4 I = src(1.0,1.0);

    vec4 J = E;
    vec4 K = E;
    vec4 L = E;
    vec4 M = E;

    frag_color = E;

    if(same(E,A) && same(E,B) && same(E,C) && same(E,D) && same(E,F) && same(E,G) && same(E,H) && same(E,I)) return;

    vec4 P  = src(0.0,2.0);
    vec4 Q  = src(-2.0,0.0);
    vec4 R  = src(2.0,0.0);
    vec4 S  = src(0.0,2.0);

    float Bl = luma(B);
    float Dl = luma(D);
    float El = luma(E);
    float Fl = luma(F);
    float Hl = luma(H);

    if (((same(D,B) && notsame(D,H) && notsame(D,F))) && ((El>=Dl) || same(E,A)) && any_eq3(E,A,C,G) && ((El<Dl) || notsame(A,D) || notsame(E,P) || notsame(E,Q))) J=mix(D, J, 0.5);
    if (((same(B,F) && notsame(B,D) && notsame(B,H))) && ((El>=Bl) || same(E,C)) && any_eq3(E,A,C,I) && ((El<Bl) || notsame(C,B) || notsame(E,P) || notsame(E,R))) K=mix(B, K, 0.5);
    if (((same(H,D) && notsame(H,F) && notsame(H,B))) && ((El>=Hl) || same(E,G)) && any_eq3(E,A,G,I) && ((El<Hl) || notsame(G,H) || notsame(E,S) || notsame(E,Q))) L=mix(H, L, 0.5);
    if (((same(F,H) && notsame(F,B) && notsame(F,D))) && ((El>=Fl) || same(E,I)) && any_eq3(E,C,G,I) && ((El<Fl) || notsame(I,H) || notsame(E,R) || notsame(E,S))) M=mix(F, M, 0.5);

    if ((notsame(E,F) && all_eq4(E,C,I,D,Q) && all_eq2(F,B,H)) && notsame(F,src(3.0,0.0))) {M=mix(M, F, 0.5); K=mix(K, M, 0.5);};
    if ((notsame(E,D) && all_eq4(E,A,G,F,R) && all_eq2(D,B,H)) && notsame(D,src(-3.0,0.0))) {L=mix(L, D, 0.5); J=mix(J, L, 0.5);};
    if ((notsame(E,H) && all_eq4(E,G,I,B,P) && all_eq2(H,D,F)) && notsame(H,src(0.0,3.0))) {M=mix(M, H, 0.5); L=mix(L, M, 0.5);};
    if ((notsame(E,B) && all_eq4(E,A,C,H,S) && all_eq2(B,D,F)) && notsame(B,src(0.0,-3.0))) {K=mix(K, B, 0.5); J=mix(J, K, 0.5);};

    if ((Bl<El) && all_eq4(E,G,H,I,S) && none_eq4(E,A,D,C,F)) {K=mix(K, B, 0.5); J=mix(J, K, 0.5);}
    if ((Hl<El) && all_eq4(E,A,B,C,P) && none_eq4(E,D,G,I,F)) {M=mix(M, H, 0.5); L=mix(L, M, 0.5);}
    if ((Fl<El) && all_eq4(E,A,D,G,Q) && none_eq4(E,B,C,I,H)) {M=mix(M, F, 0.5); K=mix(K, M, 0.5);}
    if ((Dl<El) && all_eq4(E,C,F,I,R) && none_eq4(E,B,A,G,H)) {L=mix(L, D, 0.5); J=mix(J, L, 0.5);}

    if (notsame(H,B)) {
        if (notsame(H,A) && notsame(H,E) && notsame(H,C)) {
            if (all_eq3(H,G,F,R) && none_eq2(H,D,src(2.0,-1.0))) L=mix(M, L, 0.5);
            if (all_eq3(H,I,D,Q) && none_eq2(H,F,src(-2.0,-1.0))) M=mix(L, M, 0.5);
        }

        if (notsame(B,I) && notsame(B,G) && notsame(B,E)) {
            if (all_eq3(B,A,F,R) && none_eq2(B,D,src(2.0,1.0))) J=mix(K, L, 0.5);
            if (all_eq3(B,C,D,Q) && none_eq2(B,F,src(-2.0,1.0))) K=mix(J, K, 0.5);
        }
    }

    if (notsame(F,D)) {
        if (notsame(D,I) && notsame(D,E) && notsame(D,C)) {
            if (all_eq3(D,A,H,S) && none_eq2(D,B,src(1.0,2.0))) J=mix(L, J, 0.5);
            if (all_eq3(D,G,B,P) && none_eq2(D,H,src(1.0,2.0))) L=mix(J, L, 0.5);
        }

        if (notsame(F,E) && notsame(F,A) && notsame(F,G)) {
            if (all_eq3(F,C,H,S) && none_eq2(F,B,src(-1.0,2.0))) K=mix(M, K, 0.5);
            if (all_eq3(F,I,B,P) && none_eq2(F,H,src(-1.0,-2.0))) M=mix(K, M, 0.5);
        }
    }

    vec2 a = fract(tex_coord * source_size);
    vec4 colour = (a.x < 0.5) ? (a.y < 0.5 ? J : L) : (a.y < 0.5 ? K : M);
    frag_color = colour;
}
