/*
 * freeglut_geometry.c
 *
 * Freeglut geometry rendering methods.
 *
 * Copyright (c) 1999-2000 Pawel W. Olszta. All Rights Reserved.
 * Written by Pawel W. Olszta, <olszta@sourceforge.net>
 * Creation date: Fri Dec 3 1999
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PAWEL W. OLSZTA BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <GL/freeglut.h>
#include "fg_internal.h"

/*
 *
 * Need more types of polyhedra? See CPolyhedron in MRPT
 * 
 * TODO BEFORE THE STABLE RELEASE:
 *
 * See fghTetrahedron
 *
 * Following functions have been contributed by Andreas Umbach.
 *
 *      glutWireCube()          -- looks OK
 *      glutSolidCube()         -- OK
 *
 * Those functions have been implemented by John Fay.
 *
 *      glutWireTorus()         -- looks OK
 *      glutSolidTorus()        -- looks OK
 *      glutWireDodecahedron()  -- looks OK
 *      glutSolidDodecahedron() -- looks OK
 *      glutWireOctahedron()    -- looks OK
 *      glutSolidOctahedron()   -- looks OK
 *      glutWireTetrahedron()   -- looks OK
 *      glutSolidTetrahedron()  -- looks OK
 *      glutWireIcosahedron()   -- looks OK
 *      glutSolidIcosahedron()  -- looks OK
 *
 *  The Following functions have been updated by Nigel Stewart, based
 *  on FreeGLUT 2.0.0 implementations:
 *
 *      glutWireSphere()        -- looks OK
 *      glutSolidSphere()       -- looks OK
 *      glutWireCone()          -- looks OK
 *      glutSolidCone()         -- looks OK
 */


/* General function for drawing geometry. As for all geometry we have no 
 * redundancy (or hardly any in the case of cones and cylinders) in terms
 * of the vertex/normal combinations, we just use glDrawArrays.
 * useWireMode controls the drawing of solids (false) or wire frame
 * versions (TRUE) of the geometry you pass
 */
static void fghDrawGeometry(GLenum vertexMode, GLdouble *vertices, GLdouble *normals, GLboolean *edgeFlags, GLsizei numVertices, GLboolean useWireMode)
{
    if (useWireMode)
    {
        glPushAttrib(GL_POLYGON_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (1)
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        if (edgeFlags)
            glEnableClientState(GL_EDGE_FLAG_ARRAY);

        glVertexPointer(3, GL_DOUBLE, 0, vertices);
        glNormalPointer(GL_DOUBLE, 0, normals);
        if (edgeFlags)
            glEdgeFlagPointer(0,edgeFlags);
        glDrawArrays(vertexMode, 0, numVertices);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        if (edgeFlags)
            glDisableClientState(GL_EDGE_FLAG_ARRAY);
    }
    else
    {
        int i;
        glBegin(vertexMode);
            for(i=0; i<numVertices; i++)
            {
                glEdgeFlag(edgeFlags[i]);
                glNormal3dv(normals+i*3);
                printf("n(%i) = (%1.4f,%1.4f,%1.4f)\n",i,*(normals+i*3),*(normals+i*3+1),*(normals+i*3+2));
                glVertex3dv(vertices+i*3);
                printf("v(%i) = (%1.4f,%1.4f,%1.4f)\n",i,*(vertices+i*3),*(vertices+i*3+1),*(vertices+i*3+2));
            }
        glEnd();
    }

    if (useWireMode)
    {
        glPopAttrib();
    }
}

static void fghGenerateGeometryWithEdgeFlag(int numFaces, int numEdgePerFace, GLdouble *vertices, GLubyte *vertIndices, GLdouble *normals, GLboolean *edgeFlags, GLdouble *vertOut, GLdouble *normOut, GLboolean *edgeFlagsOut)
{
    int i,j;
    /*
     * Build array with vertices from vertex coordinates and vertex indices 
     * Do same for normals.
     * Need to do this because of different normals at shared vertices
     * (and because normals' coordinates need to be negated).
     */
    for (i=0; i<numFaces; i++)
    {
        int normIdx         = i*3;
        int faceIdxVertIdx  = i*numEdgePerFace; // index to first element of "row" in vertex indices
        for (j=0; j<numEdgePerFace; j++)
        {
            int outIdx  = i*numEdgePerFace*3+j*3;
            int vertIdx = vertIndices[faceIdxVertIdx+j]*3;

            vertOut[outIdx  ] = vertices[vertIdx  ];
            vertOut[outIdx+1] = vertices[vertIdx+1];
            vertOut[outIdx+2] = vertices[vertIdx+2];

            normOut[outIdx  ] = normals [normIdx  ];
            normOut[outIdx+1] = normals [normIdx+1];
            normOut[outIdx+2] = normals [normIdx+2];

            if (edgeFlagsOut)
                edgeFlagsOut[faceIdxVertIdx+j] = edgeFlags[j];
        }
    }
}

static void fghGenerateGeometry(int numFaces, int numEdgePerFace, GLdouble *vertices, GLubyte *vertIndices, GLdouble *normals, GLdouble *vertOut, GLdouble *normOut)
{
    fghGenerateGeometryWithEdgeFlag(numFaces, numEdgePerFace, vertices, vertIndices, normals, NULL, vertOut, normOut, NULL);
}


/* -- INTERNAL SETUP OF GEOMETRY --------------------------------------- */
static unsigned int ipow (int x, unsigned int y)
{
    return y==0? 1: y==1? x: (y%2? x: 1) * ipow(x*x, y/2);
}

/* -- stuff that can be cached -- */
/* Cache of input to glDrawArrays */
#define DECLARE_SHAPE_CACHE(name,nameICaps,nameCaps)\
    static GLboolean name##Cached = FALSE;\
    static GLdouble name##_verts[nameCaps##_VERT_ELEM_PER_OBJ];\
    static GLdouble name##_norms[nameCaps##_VERT_ELEM_PER_OBJ];\
    static void fgh##nameICaps##Generate()\
    {\
        fghGenerateGeometry(nameCaps##_NUM_FACES, nameCaps##_NUM_EDGE_PER_FACE,\
                            name##_v, name##_vi, name##_n,\
                            name##_verts, name##_norms);\
    }
#define DECLARE_SHAPE_CACHE_WITH_EDGE_FLAG(name,nameICaps,nameCaps)\
    static GLboolean name##Cached = FALSE;\
    static GLdouble  name##_verts[nameCaps##_VERT_ELEM_PER_OBJ];\
    static GLdouble  name##_norms[nameCaps##_VERT_ELEM_PER_OBJ];\
    static GLboolean name##_edgeFlags[nameCaps##_VERT_PER_OBJ];\
    static void fgh##nameICaps##Generate()\
    {\
        fghGenerateGeometryWithEdgeFlag(nameCaps##_NUM_FACES, nameCaps##_NUM_EDGE_PER_FACE,\
                                        name##_v, name##_vi, name##_n, name##_ef,\
                                        name##_verts, name##_norms, name##_edgeFlags);\
    }
/*
 * In general, we build arrays with all vertices or normals.
 * We cant compress this and use glDrawElements as all combinations of
 * vertex and normals are unique.
 */

/* -- Cube -- */
#define CUBE_NUM_VERT           8
#define CUBE_NUM_FACES          6
#define CUBE_NUM_EDGE_PER_FACE  4+2   /* 1.5 is overhead factor when drawing quads as triangles */
#define CUBE_VERT_PER_OBJ       (CUBE_NUM_FACES)*(CUBE_NUM_EDGE_PER_FACE)
#define CUBE_VERT_ELEM_PER_OBJ  (CUBE_VERT_PER_OBJ)*3
/* Vertex Coordinates */
static GLdouble cube_v[CUBE_NUM_VERT*3] =
{
     .5, .5, .5,
    -.5, .5, .5,
    -.5,-.5, .5,
     .5,-.5, .5,
     .5,-.5,-.5,
     .5, .5,-.5,
    -.5, .5,-.5,
    -.5,-.5,-.5
};
/* Normal Vectors */
static GLdouble cube_n[CUBE_NUM_FACES*3] =
{
     0.0, 0.0, 1.0,
     1.0, 0.0, 0.0,
     0.0, 1.0, 0.0,
    -1.0, 0.0, 0.0,
     0.0,-1.0, 0.0,
     0.0, 0.0,-1.0
};

/* Vertex indices */
static GLubyte cube_vi[CUBE_VERT_PER_OBJ] =
{
    0,1,2,0,2,3,
    0,3,4,0,4,5,
    0,5,6,0,6,1,
    1,6,7,1,7,2,
    7,4,3,7,3,2,
    4,7,6,4,6,5
};
/* edge flags */
static GLboolean cube_ef[CUBE_NUM_EDGE_PER_FACE] =
{
    1,1,0,0,1,1
};
DECLARE_SHAPE_CACHE_WITH_EDGE_FLAG(cube,Cube,CUBE);

/* Icosahedron */
#define ICOSAHEDRON_NUM_VERT           12
#define ICOSAHEDRON_NUM_FACES          20
#define ICOSAHEDRON_NUM_EDGE_PER_FACE  3
#define ICOSAHEDRON_VERT_PER_OBJ       ICOSAHEDRON_NUM_FACES*ICOSAHEDRON_NUM_EDGE_PER_FACE
#define ICOSAHEDRON_VERT_ELEM_PER_OBJ  ICOSAHEDRON_VERT_PER_OBJ*3
/* Vertex Coordinates */
static GLdouble icosahedron_v[ICOSAHEDRON_NUM_VERT*3] =
{
     1.0,             0.0,             0.0           ,
     0.447213595500,  0.894427191000,  0.0           ,
     0.447213595500,  0.276393202252,  0.850650808354,
     0.447213595500, -0.723606797748,  0.525731112119,
     0.447213595500, -0.723606797748, -0.525731112119,
     0.447213595500,  0.276393202252, -0.850650808354,
    -0.447213595500, -0.894427191000,  0.0           ,
    -0.447213595500, -0.276393202252,  0.850650808354,
    -0.447213595500,  0.723606797748,  0.525731112119,
    -0.447213595500,  0.723606797748, -0.525731112119,
    -0.447213595500, -0.276393202252, -0.850650808354,
    -1.0,             0.0,             0.0           
};
/* Normal Vectors:
 * icosahedron_n[i][0] = ( icosahedron_v[icosahedron_vi[i][1]][1] - icosahedron_v[icosahedron_vi[i][0]][1] ) * ( icosahedron_v[icosahedron_vi[i][2]][2] - icosahedron_v[icosahedron_vi[i][0]][2] ) - ( icosahedron_v[icosahedron_vi[i][1]][2] - icosahedron_v[icosahedron_vi[i][0]][2] ) * ( icosahedron_v[icosahedron_vi[i][2]][1] - icosahedron_v[icosahedron_vi[i][0]][1] ) ;
 * icosahedron_n[i][1] = ( icosahedron_v[icosahedron_vi[i][1]][2] - icosahedron_v[icosahedron_vi[i][0]][2] ) * ( icosahedron_v[icosahedron_vi[i][2]][0] - icosahedron_v[icosahedron_vi[i][0]][0] ) - ( icosahedron_v[icosahedron_vi[i][1]][0] - icosahedron_v[icosahedron_vi[i][0]][0] ) * ( icosahedron_v[icosahedron_vi[i][2]][2] - icosahedron_v[icosahedron_vi[i][0]][2] ) ;
 * icosahedron_n[i][2] = ( icosahedron_v[icosahedron_vi[i][1]][0] - icosahedron_v[icosahedron_vi[i][0]][0] ) * ( icosahedron_v[icosahedron_vi[i][2]][1] - icosahedron_v[icosahedron_vi[i][0]][1] ) - ( icosahedron_v[icosahedron_vi[i][1]][1] - icosahedron_v[icosahedron_vi[i][0]][1] ) * ( icosahedron_v[icosahedron_vi[i][2]][0] - icosahedron_v[icosahedron_vi[i][0]][0] ) ;
*/
static GLdouble icosahedron_n[ICOSAHEDRON_NUM_FACES*3] =
{
     0.760845213037948,  0.470228201835026,  0.341640786498800,
     0.760845213036861, -0.179611190632978,  0.552786404500000,
     0.760845213033849, -0.581234022404097,                  0,
     0.760845213036861, -0.179611190632978, -0.552786404500000,
     0.760845213037948,  0.470228201835026, -0.341640786498800,
     0.179611190628666,  0.760845213037948,  0.552786404498399,
     0.179611190634277, -0.290617011204044,  0.894427191000000,
     0.179611190633958, -0.940456403667806,                  0,
     0.179611190634278, -0.290617011204044, -0.894427191000000,
     0.179611190628666,  0.760845213037948, -0.552786404498399,
    -0.179611190633958,  0.940456403667806,                  0,
    -0.179611190634277,  0.290617011204044,  0.894427191000000,
    -0.179611190628666, -0.760845213037948,  0.552786404498399,
    -0.179611190628666, -0.760845213037948, -0.552786404498399,
    -0.179611190634277,  0.290617011204044, -0.894427191000000,
    -0.760845213036861,  0.179611190632978, -0.552786404500000,
    -0.760845213033849,  0.581234022404097,                  0,
    -0.760845213036861,  0.179611190632978,  0.552786404500000,
    -0.760845213037948, -0.470228201835026,  0.341640786498800,
    -0.760845213037948, -0.470228201835026, -0.341640786498800,
};

/* Vertex indices */
static GLubyte icosahedron_vi[ICOSAHEDRON_VERT_PER_OBJ] =
{
    0,   1,  2 ,
    0,   2,  3 ,
    0,   3,  4 ,
    0,   4,  5 ,
    0,   5,  1 ,
    1,   8,  2 ,
    2,   7,  3 ,
    3,   6,  4 ,
    4,  10,  5 ,
    5,   9,  1 ,
    1,   9,  8 ,
    2,   8,  7 ,
    3,   7,  6 ,
    4,   6, 10 ,
    5,  10,  9 ,
    11,  9, 10 ,
    11,  8,  9 ,
    11,  7,  8 ,
    11,  6,  7 ,
    11, 10,  6 
};
DECLARE_SHAPE_CACHE(icosahedron,Icosahedron,ICOSAHEDRON);

/* -- Octahedron -- */
#define OCTAHEDRON_NUM_VERT           6
#define OCTAHEDRON_NUM_FACES          8
#define OCTAHEDRON_NUM_EDGE_PER_FACE  3
#define OCTAHEDRON_VERT_PER_OBJ       OCTAHEDRON_NUM_FACES*OCTAHEDRON_NUM_EDGE_PER_FACE
#define OCTAHEDRON_VERT_ELEM_PER_OBJ  OCTAHEDRON_VERT_PER_OBJ*3

/* Vertex Coordinates */
static GLdouble octahedron_v[OCTAHEDRON_NUM_VERT*3] =
{
     1.,  0.,  0.,
     0.,  1.,  0.,
     0.,  0.,  1.,
    -1.,  0.,  0.,
     0., -1.,  0.,
     0.,  0., -1.,

};
/* Normal Vectors */
static GLdouble octahedron_n[OCTAHEDRON_NUM_FACES*3] =
{
     0.577350269189, 0.577350269189, 0.577350269189,    /* sqrt(1/3) */
     0.577350269189, 0.577350269189,-0.577350269189,
     0.577350269189,-0.577350269189, 0.577350269189,
     0.577350269189,-0.577350269189,-0.577350269189,
    -0.577350269189, 0.577350269189, 0.577350269189,
    -0.577350269189, 0.577350269189,-0.577350269189,
    -0.577350269189,-0.577350269189, 0.577350269189,
    -0.577350269189,-0.577350269189,-0.577350269189

};

/* Vertex indices */
static GLubyte octahedron_vi[OCTAHEDRON_VERT_PER_OBJ] =
{
    0, 1, 2,
    0, 5, 1,
    0, 2, 4,
    0, 4, 5,
    3, 2, 1,
    3, 1, 5,
    3, 4, 2,
    3, 5, 4
};
DECLARE_SHAPE_CACHE(octahedron,Octahedron,OCTAHEDRON);

/* -- RhombicDodecahedron -- */
#define RHOMBICDODECAHEDRON_NUM_VERT            14
#define RHOMBICDODECAHEDRON_NUM_FACES           12
#define RHOMBICDODECAHEDRON_NUM_EDGE_PER_FACE   4
#define RHOMBICDODECAHEDRON_VERT_PER_OBJ        RHOMBICDODECAHEDRON_NUM_FACES*RHOMBICDODECAHEDRON_NUM_EDGE_PER_FACE
#define RHOMBICDODECAHEDRON_VERT_ELEM_PER_OBJ   RHOMBICDODECAHEDRON_VERT_PER_OBJ*3

/* Vertex Coordinates */
static GLdouble rhombicdodecahedron_v[RHOMBICDODECAHEDRON_NUM_VERT*3] =
{
     0.0,             0.0,             1.0,
     0.707106781187,  0.0           ,  0.5,
     0.0           ,  0.707106781187,  0.5,
    -0.707106781187,  0.0           ,  0.5,
     0.0           , -0.707106781187,  0.5,
     0.707106781187,  0.707106781187,  0.0,
    -0.707106781187,  0.707106781187,  0.0,
    -0.707106781187, -0.707106781187,  0.0,
     0.707106781187, -0.707106781187,  0.0,
     0.707106781187,  0.0           , -0.5,
     0.0           ,  0.707106781187, -0.5,
    -0.707106781187,  0.0           , -0.5,
     0.0           , -0.707106781187, -0.5,
     0.0,             0.0,            -1.0
};
/* Normal Vectors */
static GLdouble rhombicdodecahedron_n[RHOMBICDODECAHEDRON_NUM_FACES*3] =
{
     0.353553390594,  0.353553390594,  0.5,
    -0.353553390594,  0.353553390594,  0.5,
    -0.353553390594, -0.353553390594,  0.5,
     0.353553390594, -0.353553390594,  0.5,
     0.0           ,  1.0           ,  0.0,
    -1.0           ,  0.0           ,  0.0,
     0.0           , -1.0           ,  0.0,
     1.0           ,  0.0           ,  0.0,
     0.353553390594,  0.353553390594, -0.5,
    -0.353553390594,  0.353553390594, -0.5,
    -0.353553390594, -0.353553390594, -0.5,
     0.353553390594, -0.353553390594, -0.5
};

/* Vertex indices */
static GLubyte rhombicdodecahedron_vi[RHOMBICDODECAHEDRON_VERT_PER_OBJ] =
{
    0,  1,  5,  2,
    0,  2,  6,  3,
    0,  3,  7,  4,
    0,  4,  8,  1,
    5, 10,  6,  2,
    6, 11,  7,  3,
    7, 12,  8,  4,
    8,  9,  5,  1,
    5,  9, 13, 10,
    6, 10, 13, 11,
    7, 11, 13, 12,
    8, 12, 13,  9
};
DECLARE_SHAPE_CACHE(rhombicdodecahedron,RhombicDodecahedron,RHOMBICDODECAHEDRON);

/* -- Tetrahedron -- */
/* Magic Numbers:  r0 = ( 1, 0, 0 )
 *                 r1 = ( -1/3, 2 sqrt(2) / 3, 0 )
 *                 r2 = ( -1/3, - sqrt(2) / 3,  sqrt(6) / 3 )
 *                 r3 = ( -1/3, - sqrt(2) / 3, -sqrt(6) / 3 )
 * |r0| = |r1| = |r2| = |r3| = 1
 * Distance between any two points is 2 sqrt(6) / 3
 *
 * Normals:  The unit normals are simply the negative of the coordinates of the point not on the surface.
 */
#define TETRAHEDRON_NUM_VERT            4
#define TETRAHEDRON_NUM_FACES           4
#define TETRAHEDRON_NUM_EDGE_PER_FACE   3
#define TETRAHEDRON_VERT_PER_OBJ        TETRAHEDRON_NUM_FACES*TETRAHEDRON_NUM_EDGE_PER_FACE
#define TETRAHEDRON_VERT_ELEM_PER_OBJ   TETRAHEDRON_VERT_PER_OBJ*3

/* Vertex Coordinates */
static GLdouble tetrahedron_v[TETRAHEDRON_NUM_VERT*3] =
{
                1.0,             0.0,             0.0,
    -0.333333333333,  0.942809041582,             0.0,
    -0.333333333333, -0.471404520791,  0.816496580928,
    -0.333333333333, -0.471404520791, -0.816496580928
};
/* Normal Vectors */
static GLdouble tetrahedron_n[TETRAHEDRON_NUM_FACES*3] =
{
    -           1.0,             0.0,             0.0,
     0.333333333333, -0.942809041582,             0.0,
     0.333333333333,  0.471404520791, -0.816496580928,
     0.333333333333,  0.471404520791,  0.816496580928
};

/* Vertex indices */
static GLubyte tetrahedron_vi[TETRAHEDRON_VERT_PER_OBJ] =
{
    1, 3, 2,
    0, 2, 3,
    0, 3, 1,
    0, 1, 2
};
DECLARE_SHAPE_CACHE(tetrahedron,Tetrahedron,TETRAHEDRON);

/* -- Sierpinski Sponge -- */
static void fghSierpinskiSpongeGenerate ( int numLevels, GLdouble offset[3], GLdouble scale, GLdouble* vertices, GLdouble* normals )
{
    int i, j;
    if ( numLevels == 0 )
    {
        for (i=0; i<TETRAHEDRON_NUM_FACES; i++)
        {
            int normIdx         = i*3;
            int faceIdxVertIdx  = i*TETRAHEDRON_NUM_EDGE_PER_FACE;
            for (j=0; j<TETRAHEDRON_NUM_EDGE_PER_FACE; j++)
            {
                int outIdx  = i*TETRAHEDRON_NUM_EDGE_PER_FACE*3+j*3;
                int vertIdx = tetrahedron_vi[faceIdxVertIdx+j]*3;

                vertices[outIdx  ] = offset[0] + scale * tetrahedron_v[vertIdx  ];
                vertices[outIdx+1] = offset[1] + scale * tetrahedron_v[vertIdx+1];
                vertices[outIdx+2] = offset[2] + scale * tetrahedron_v[vertIdx+2];

                normals [outIdx  ] = tetrahedron_n[normIdx  ];
                normals [outIdx+1] = tetrahedron_n[normIdx+1];
                normals [outIdx+2] = tetrahedron_n[normIdx+2];
            }
        }
    }
    else if ( numLevels > 0 )
    {
        GLdouble local_offset[3] ;  /* Use a local variable to avoid buildup of roundoff errors */
        unsigned int stride = ipow(4,--numLevels)*TETRAHEDRON_VERT_ELEM_PER_OBJ;
        scale /= 2.0 ;
        for ( i = 0 ; i < TETRAHEDRON_NUM_FACES ; i++ )
        {
            int idx         = i*3;
            local_offset[0] = offset[0] + scale * tetrahedron_v[idx  ];
            local_offset[1] = offset[1] + scale * tetrahedron_v[idx+1];
            local_offset[2] = offset[2] + scale * tetrahedron_v[idx+2];
            fghSierpinskiSpongeGenerate ( numLevels, local_offset, scale, vertices+i*stride, normals+i*stride );
        }
    }
}

/* -- Now the various shapes involving circles -- */
/*
 * Compute lookup table of cos and sin values forming a cirle
 *
 * Notes:
 *    It is the responsibility of the caller to free these tables
 *    The size of the table is (n+1) to form a connected loop
 *    The last entry is exactly the same as the first
 *    The sign of n can be flipped to get the reverse loop
 */
static void fghCircleTable(double **sint,double **cost,const int n)
{
    int i;

    /* Table size, the sign of n flips the circle direction */

    const int size = abs(n);

    /* Determine the angle between samples */

    const double angle = 2*M_PI/(double)( ( n == 0 ) ? 1 : n );

    /* Allocate memory for n samples, plus duplicate of first entry at the end */

    *sint = (double *) calloc(sizeof(double), size+1);
    *cost = (double *) calloc(sizeof(double), size+1);

    /* Bail out if memory allocation fails, fgError never returns */

    if (!(*sint) || !(*cost))
    {
        free(*sint);
        free(*cost);
        fgError("Failed to allocate memory in fghCircleTable");
    }

    /* Compute cos and sin around the circle */

    (*sint)[0] = 0.0;
    (*cost)[0] = 1.0;

    for (i=1; i<size; i++)
    {
        (*sint)[i] = sin(angle*i);
        (*cost)[i] = cos(angle*i);
    }

    /* Last sample is duplicate of the first */

    (*sint)[size] = (*sint)[0];
    (*cost)[size] = (*cost)[0];
}


/* -- INTERNAL DRAWING functions to avoid code duplication ------------- */
#define DECLARE_INTERNAL_DRAW(vertexMode,name,nameICaps,nameCaps)\
    static void fgh##nameICaps( GLboolean useWireMode )\
    {\
        if (!name##Cached)\
        {\
            fgh##nameICaps##Generate();\
            name##Cached = GL_TRUE;\
        }\
        fghDrawGeometry(vertexMode,name##_verts,name##_norms,NULL,nameCaps##_VERT_PER_OBJ,useWireMode);\
    }

static void fghCube( GLdouble dSize, GLboolean useWireMode )
{
    if (!cubeCached)
    {
        fghCubeGenerate();
        cubeCached = GL_TRUE;
    }

    if (dSize!=1.)
    {
        int i;

        /* Need to build new vertex list containing vertices for cube of different size */
        GLdouble *vertices = malloc(CUBE_VERT_ELEM_PER_OBJ * sizeof(GLdouble));
        for (i=0; i<CUBE_VERT_ELEM_PER_OBJ; i++)
            vertices[i] = dSize*cube_verts[i];

        fghDrawGeometry(GL_TRIANGLES,vertices  ,cube_norms,cube_edgeFlags,CUBE_VERT_PER_OBJ,useWireMode);
    }
    else
        fghDrawGeometry(GL_TRIANGLES,cube_verts,cube_norms,cube_edgeFlags,CUBE_VERT_PER_OBJ,useWireMode);
}

DECLARE_INTERNAL_DRAW(GL_TRIANGLES,icosahedron,Icosahedron,ICOSAHEDRON);
DECLARE_INTERNAL_DRAW(GL_TRIANGLES,octahedron,Octahedron,OCTAHEDRON);
DECLARE_INTERNAL_DRAW(GL_QUADS,rhombicdodecahedron,RhombicDodecahedron,RHOMBICDODECAHEDRON);
DECLARE_INTERNAL_DRAW(GL_TRIANGLES,tetrahedron,Tetrahedron,TETRAHEDRON);

static void fghSierpinskiSponge ( int numLevels, GLdouble offset[3], GLdouble scale, GLboolean useWireMode )
{
    GLdouble *vertices;
    GLdouble * normals;
    GLsizei    numTetr = numLevels<0? 0 : ipow(4,numLevels); /* No sponge for numLevels below 0 */
    GLsizei    numVert = numTetr*TETRAHEDRON_VERT_PER_OBJ;

    if (numTetr)
    {
        /* Allocate memory */
        vertices = malloc(numVert*3 * sizeof(GLdouble));
        normals  = malloc(numVert*3 * sizeof(GLdouble));

        /* Generate elements */
        fghSierpinskiSpongeGenerate ( numLevels, offset, scale, vertices, normals );

        /* Draw and cleanup */
        fghDrawGeometry(GL_TRIANGLES,vertices,normals,NULL,numVert,useWireMode);
        free(vertices);
        free(normals );
    }
}


/* -- INTERFACE FUNCTIONS ---------------------------------------------- */


/*
 * Draws a solid sphere
 */
void FGAPIENTRY glutSolidSphere(GLdouble radius, GLint slices, GLint stacks)
{
    int i,j;

    /* Adjust z and radius as stacks are drawn. */

    double z0,z1;
    double r0,r1;

    /* Pre-computed circle */

    double *sint1,*cost1;
    double *sint2,*cost2;

    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolidSphere" );

    fghCircleTable(&sint1,&cost1,-slices);
    fghCircleTable(&sint2,&cost2,stacks*2);

    /* The top stack is covered with a triangle fan */

    z0 = 1.0;
    z1 = cost2[(stacks>0)?1:0];
    r0 = 0.0;
    r1 = sint2[(stacks>0)?1:0];

    glBegin(GL_TRIANGLE_FAN);

        glNormal3d(0,0,1);
        glVertex3d(0,0,radius);

        for (j=slices; j>=0; j--)
        {
            glNormal3d(cost1[j]*r1,        sint1[j]*r1,        z1       );
            glVertex3d(cost1[j]*r1*radius, sint1[j]*r1*radius, z1*radius);
        }

    glEnd();

    /* Cover each stack with a quad strip, except the top and bottom stacks */

    for( i=1; i<stacks-1; i++ )
    {
        z0 = z1; z1 = cost2[i+1];
        r0 = r1; r1 = sint2[i+1];

        glBegin(GL_QUAD_STRIP);

            for(j=0; j<=slices; j++)
            {
                glNormal3d(cost1[j]*r1,        sint1[j]*r1,        z1       );
                glVertex3d(cost1[j]*r1*radius, sint1[j]*r1*radius, z1*radius);
                glNormal3d(cost1[j]*r0,        sint1[j]*r0,        z0       );
                glVertex3d(cost1[j]*r0*radius, sint1[j]*r0*radius, z0*radius);
            }

        glEnd();
    }

    /* The bottom stack is covered with a triangle fan */

    z0 = z1;
    r0 = r1;

    glBegin(GL_TRIANGLE_FAN);

        glNormal3d(0,0,-1);
        glVertex3d(0,0,-radius);

        for (j=0; j<=slices; j++)
        {
            glNormal3d(cost1[j]*r0,        sint1[j]*r0,        z0       );
            glVertex3d(cost1[j]*r0*radius, sint1[j]*r0*radius, z0*radius);
        }

    glEnd();

    /* Release sin and cos tables */

    free(sint1);
    free(cost1);
    free(sint2);
    free(cost2);
}

/*
 * Draws a wire sphere
 */
void FGAPIENTRY glutWireSphere(GLdouble radius, GLint slices, GLint stacks)
{
    int i,j;

    /* Adjust z and radius as stacks and slices are drawn. */

    double r;
    double x,y,z;

    /* Pre-computed circle */

    double *sint1,*cost1;
    double *sint2,*cost2;

    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWireSphere" );

    fghCircleTable(&sint1,&cost1,-slices  );
    fghCircleTable(&sint2,&cost2, stacks*2);

    /* Draw a line loop for each stack */

    for (i=1; i<stacks; i++)
    {
        z = cost2[i];
        r = sint2[i];

        glBegin(GL_LINE_LOOP);

            for(j=0; j<=slices; j++)
            {
                x = cost1[j];
                y = sint1[j];

                glNormal3d(x,y,z);
                glVertex3d(x*r*radius,y*r*radius,z*radius);
            }

        glEnd();
    }

    /* Draw a line loop for each slice */

    for (i=0; i<slices; i++)
    {
        glBegin(GL_LINE_STRIP);

            for(j=0; j<=stacks; j++)
            {
                x = cost1[i]*sint2[j];
                y = sint1[i]*sint2[j];
                z = cost2[j];

                glNormal3d(x,y,z);
                glVertex3d(x*radius,y*radius,z*radius);
            }

        glEnd();
    }

    /* Release sin and cos tables */

    free(sint1);
    free(cost1);
    free(sint2);
    free(cost2);
}

/*
 * Draws a solid cone
 */
void FGAPIENTRY glutSolidCone( GLdouble base, GLdouble height, GLint slices, GLint stacks )
{
    int i,j;

    /* Step in z and radius as stacks are drawn. */

    double z0,z1;
    double r0,r1;

    const double zStep = height / ( ( stacks > 0 ) ? stacks : 1 );
    const double rStep = base / ( ( stacks > 0 ) ? stacks : 1 );

    /* Scaling factors for vertex normals */

    const double cosn = ( height / sqrt ( height * height + base * base ));
    const double sinn = ( base   / sqrt ( height * height + base * base ));

    /* Pre-computed circle */

    double *sint,*cost;

    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolidCone" );

    fghCircleTable(&sint,&cost,-slices);

    /* Cover the circular base with a triangle fan... */

    z0 = 0.0;
    z1 = zStep;

    r0 = base;
    r1 = r0 - rStep;

    glBegin(GL_TRIANGLE_FAN);

        glNormal3d(0.0,0.0,-1.0);
        glVertex3d(0.0,0.0, z0 );

        for (j=0; j<=slices; j++)
            glVertex3d(cost[j]*r0, sint[j]*r0, z0);

    glEnd();

    /* Cover each stack with a quad strip, except the top stack */

    for( i=0; i<stacks-1; i++ )
    {
        glBegin(GL_QUAD_STRIP);

            for(j=0; j<=slices; j++)
            {
                glNormal3d(cost[j]*cosn, sint[j]*cosn, sinn);
                glVertex3d(cost[j]*r0,   sint[j]*r0,   z0  );
                glVertex3d(cost[j]*r1,   sint[j]*r1,   z1  );
            }

            z0 = z1; z1 += zStep;
            r0 = r1; r1 -= rStep;

        glEnd();
    }

    /* The top stack is covered with individual triangles */

    glBegin(GL_TRIANGLES);

        glNormal3d(cost[0]*sinn, sint[0]*sinn, cosn);

        for (j=0; j<slices; j++)
        {
            glVertex3d(cost[j+0]*r0,   sint[j+0]*r0,   z0    );
            glVertex3d(0,              0,              height);
            glNormal3d(cost[j+1]*sinn, sint[j+1]*sinn, cosn  );
            glVertex3d(cost[j+1]*r0,   sint[j+1]*r0,   z0    );
        }

    glEnd();

    /* Release sin and cos tables */

    free(sint);
    free(cost);
}

/*
 * Draws a wire cone
 */
void FGAPIENTRY glutWireCone( GLdouble base, GLdouble height, GLint slices, GLint stacks)
{
    int i,j;

    /* Step in z and radius as stacks are drawn. */

    double z = 0.0;
    double r = base;

    const double zStep = height / ( ( stacks > 0 ) ? stacks : 1 );
    const double rStep = base / ( ( stacks > 0 ) ? stacks : 1 );

    /* Scaling factors for vertex normals */

    const double cosn = ( height / sqrt ( height * height + base * base ));
    const double sinn = ( base   / sqrt ( height * height + base * base ));

    /* Pre-computed circle */

    double *sint,*cost;

    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWireCone" );

    fghCircleTable(&sint,&cost,-slices);

    /* Draw the stacks... */

    for (i=0; i<stacks; i++)
    {
        glBegin(GL_LINE_LOOP);

            for( j=0; j<slices; j++ )
            {
                glNormal3d(cost[j]*sinn, sint[j]*sinn, cosn);
                glVertex3d(cost[j]*r,    sint[j]*r,    z   );
            }

        glEnd();

        z += zStep;
        r -= rStep;
    }

    /* Draw the slices */

    r = base;

    glBegin(GL_LINES);

        for (j=0; j<slices; j++)
        {
            glNormal3d(cost[j]*sinn, sint[j]*sinn, cosn  );
            glVertex3d(cost[j]*r,    sint[j]*r,    0.0   );
            glVertex3d(0.0,          0.0,          height);
        }

    glEnd();

    /* Release sin and cos tables */

    free(sint);
    free(cost);
}


/*
 * Draws a solid cylinder
 */
void FGAPIENTRY glutSolidCylinder(GLdouble radius, GLdouble height, GLint slices, GLint stacks)
{
    int i,j;

    /* Step in z and radius as stacks are drawn. */

    double z0,z1;
    const double zStep = height / ( ( stacks > 0 ) ? stacks : 1 );

    /* Pre-computed circle */

    double *sint,*cost;

    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolidCylinder" );

    fghCircleTable(&sint,&cost,-slices);

    /* Cover the base and top */

    glBegin(GL_TRIANGLE_FAN);
        glNormal3d(0.0, 0.0, -1.0 );
        glVertex3d(0.0, 0.0,  0.0 );
        for (j=0; j<=slices; j++)
          glVertex3d(cost[j]*radius, sint[j]*radius, 0.0);
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
        glNormal3d(0.0, 0.0, 1.0   );
        glVertex3d(0.0, 0.0, height);
        for (j=slices; j>=0; j--)
          glVertex3d(cost[j]*radius, sint[j]*radius, height);
    glEnd();

    /* Do the stacks */

    z0 = 0.0;
    z1 = zStep;

    for (i=1; i<=stacks; i++)
    {
        if (i==stacks)
            z1 = height;

        glBegin(GL_QUAD_STRIP);
            for (j=0; j<=slices; j++ )
            {
                glNormal3d(cost[j],        sint[j],        0.0 );
                glVertex3d(cost[j]*radius, sint[j]*radius, z0  );
                glVertex3d(cost[j]*radius, sint[j]*radius, z1  );
            }
        glEnd();

        z0 = z1; z1 += zStep;
    }

    /* Release sin and cos tables */

    free(sint);
    free(cost);
}

/*
 * Draws a wire cylinder
 */
void FGAPIENTRY glutWireCylinder(GLdouble radius, GLdouble height, GLint slices, GLint stacks)
{
    int i,j;

    /* Step in z and radius as stacks are drawn. */

          double z = 0.0;
    const double zStep = height / ( ( stacks > 0 ) ? stacks : 1 );

    /* Pre-computed circle */

    double *sint,*cost;

    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWireCylinder" );

    fghCircleTable(&sint,&cost,-slices);

    /* Draw the stacks... */

    for (i=0; i<=stacks; i++)
    {
        if (i==stacks)
            z = height;

        glBegin(GL_LINE_LOOP);

            for( j=0; j<slices; j++ )
            {
                glNormal3d(cost[j],        sint[j],        0.0);
                glVertex3d(cost[j]*radius, sint[j]*radius, z  );
            }

        glEnd();

        z += zStep;
    }

    /* Draw the slices */

    glBegin(GL_LINES);

        for (j=0; j<slices; j++)
        {
            glNormal3d(cost[j],        sint[j],        0.0   );
            glVertex3d(cost[j]*radius, sint[j]*radius, 0.0   );
            glVertex3d(cost[j]*radius, sint[j]*radius, height);
        }

    glEnd();

    /* Release sin and cos tables */

    free(sint);
    free(cost);
}

/*
 * Draws a wire torus
 */
void FGAPIENTRY glutWireTorus( GLdouble dInnerRadius, GLdouble dOuterRadius, GLint nSides, GLint nRings )
{
  double  iradius = dInnerRadius, oradius = dOuterRadius, phi, psi, dpsi, dphi;
  double *vertex, *normal;
  int    i, j;
  double spsi, cpsi, sphi, cphi ;

  FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWireTorus" );

  if ( nSides < 1 ) nSides = 1;
  if ( nRings < 1 ) nRings = 1;

  /* Allocate the vertices array */
  vertex = (double *)calloc( sizeof(double), 3 * nSides * nRings );
  normal = (double *)calloc( sizeof(double), 3 * nSides * nRings );

  glPushMatrix();

  dpsi =  2.0 * M_PI / (double)nRings ;
  dphi = -2.0 * M_PI / (double)nSides ;
  psi  = 0.0;

  for( j=0; j<nRings; j++ )
  {
    cpsi = cos ( psi ) ;
    spsi = sin ( psi ) ;
    phi = 0.0;

    for( i=0; i<nSides; i++ )
    {
      int offset = 3 * ( j * nSides + i ) ;
      cphi = cos ( phi ) ;
      sphi = sin ( phi ) ;
      *(vertex + offset + 0) = cpsi * ( oradius + cphi * iradius ) ;
      *(vertex + offset + 1) = spsi * ( oradius + cphi * iradius ) ;
      *(vertex + offset + 2) =                    sphi * iradius  ;
      *(normal + offset + 0) = cpsi * cphi ;
      *(normal + offset + 1) = spsi * cphi ;
      *(normal + offset + 2) =        sphi ;
      phi += dphi;
    }

    psi += dpsi;
  }

  for( i=0; i<nSides; i++ )
  {
    glBegin( GL_LINE_LOOP );

    for( j=0; j<nRings; j++ )
    {
      int offset = 3 * ( j * nSides + i ) ;
      glNormal3dv( normal + offset );
      glVertex3dv( vertex + offset );
    }

    glEnd();
  }

  for( j=0; j<nRings; j++ )
  {
    glBegin(GL_LINE_LOOP);

    for( i=0; i<nSides; i++ )
    {
      int offset = 3 * ( j * nSides + i ) ;
      glNormal3dv( normal + offset );
      glVertex3dv( vertex + offset );
    }

    glEnd();
  }

  free ( vertex ) ;
  free ( normal ) ;
  glPopMatrix();
}

/*
 * Draws a solid torus
 */
void FGAPIENTRY glutSolidTorus( GLdouble dInnerRadius, GLdouble dOuterRadius, GLint nSides, GLint nRings )
{
  double  iradius = dInnerRadius, oradius = dOuterRadius, phi, psi, dpsi, dphi;
  double *vertex, *normal;
  int    i, j;
  double spsi, cpsi, sphi, cphi ;

  FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolidTorus" );

  if ( nSides < 1 ) nSides = 1;
  if ( nRings < 1 ) nRings = 1;

  /* Increment the number of sides and rings to allow for one more point than surface */
  nSides ++ ;
  nRings ++ ;

  /* Allocate the vertices array */
  vertex = (double *)calloc( sizeof(double), 3 * nSides * nRings );
  normal = (double *)calloc( sizeof(double), 3 * nSides * nRings );

  glPushMatrix();

  dpsi =  2.0 * M_PI / (double)(nRings - 1) ;
  dphi = -2.0 * M_PI / (double)(nSides - 1) ;
  psi  = 0.0;

  for( j=0; j<nRings; j++ )
  {
    cpsi = cos ( psi ) ;
    spsi = sin ( psi ) ;
    phi = 0.0;

    for( i=0; i<nSides; i++ )
    {
      int offset = 3 * ( j * nSides + i ) ;
      cphi = cos ( phi ) ;
      sphi = sin ( phi ) ;
      *(vertex + offset + 0) = cpsi * ( oradius + cphi * iradius ) ;
      *(vertex + offset + 1) = spsi * ( oradius + cphi * iradius ) ;
      *(vertex + offset + 2) =                    sphi * iradius  ;
      *(normal + offset + 0) = cpsi * cphi ;
      *(normal + offset + 1) = spsi * cphi ;
      *(normal + offset + 2) =        sphi ;
      phi += dphi;
    }

    psi += dpsi;
  }

    glBegin( GL_QUADS );
  for( i=0; i<nSides-1; i++ )
  {
    for( j=0; j<nRings-1; j++ )
    {
      int offset = 3 * ( j * nSides + i ) ;
      glNormal3dv( normal + offset );
      glVertex3dv( vertex + offset );
      glNormal3dv( normal + offset + 3 );
      glVertex3dv( vertex + offset + 3 );
      glNormal3dv( normal + offset + 3 * nSides + 3 );
      glVertex3dv( vertex + offset + 3 * nSides + 3 );
      glNormal3dv( normal + offset + 3 * nSides );
      glVertex3dv( vertex + offset + 3 * nSides );
    }
  }

  glEnd();

  free ( vertex ) ;
  free ( normal ) ;
  glPopMatrix();
}

/*
 *
 */
void FGAPIENTRY glutWireDodecahedron( void )
{
  FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWireDodecahedron" );

  /* Magic Numbers:  It is possible to create a dodecahedron by attaching two pentagons to each face of
   * of a cube.  The coordinates of the points are:
   *   (+-x,0, z); (+-1, 1, 1); (0, z, x )
   * where x = (-1 + sqrt(5))/2, z = (1 + sqrt(5))/2  or
   *       x = 0.61803398875 and z = 1.61803398875.
   */
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.0,  0.525731112119,  0.850650808354 ) ; glVertex3d (  0.0,  1.61803398875,  0.61803398875 ) ; glVertex3d ( -1.0,  1.0,  1.0 ) ; glVertex3d ( -0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d (  0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d (  1.0,  1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.0,  0.525731112119, -0.850650808354 ) ; glVertex3d (  0.0,  1.61803398875, -0.61803398875 ) ; glVertex3d (  1.0,  1.0, -1.0 ) ; glVertex3d (  0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d ( -0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d ( -1.0,  1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.0, -0.525731112119,  0.850650808354 ) ; glVertex3d (  0.0, -1.61803398875,  0.61803398875 ) ; glVertex3d (  1.0, -1.0,  1.0 ) ; glVertex3d (  0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d ( -0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d ( -1.0, -1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.0, -0.525731112119, -0.850650808354 ) ; glVertex3d (  0.0, -1.61803398875, -0.61803398875 ) ; glVertex3d ( -1.0, -1.0, -1.0 ) ; glVertex3d ( -0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d (  0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d (  1.0, -1.0, -1.0 ) ;
  glEnd () ;

  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.850650808354,  0.0,  0.525731112119 ) ; glVertex3d (  0.61803398875,  0.0,  1.61803398875 ) ; glVertex3d (  1.0, -1.0,  1.0 ) ; glVertex3d (  1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d (  1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d (  1.0,  1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d ( -0.850650808354,  0.0,  0.525731112119 ) ; glVertex3d ( -0.61803398875,  0.0,  1.61803398875 ) ; glVertex3d ( -1.0,  1.0,  1.0 ) ; glVertex3d ( -1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d ( -1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d ( -1.0, -1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.850650808354,  0.0, -0.525731112119 ) ; glVertex3d (  0.61803398875,  0.0, -1.61803398875 ) ; glVertex3d (  1.0,  1.0, -1.0 ) ; glVertex3d (  1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d (  1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d (  1.0, -1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d ( -0.850650808354,  0.0, -0.525731112119 ) ; glVertex3d ( -0.61803398875,  0.0, -1.61803398875 ) ; glVertex3d ( -1.0, -1.0, -1.0 ) ; glVertex3d ( -1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d ( -1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d ( -1.0,  1.0, -1.0 ) ;
  glEnd () ;

  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.525731112119,  0.850650808354,  0.0 ) ; glVertex3d (  1.61803398875,  0.61803398875,  0.0 ) ; glVertex3d (  1.0,  1.0, -1.0 ) ; glVertex3d ( 0.0,  1.61803398875, -0.61803398875 ) ; glVertex3d ( 0.0,  1.61803398875,  0.61803398875 ) ; glVertex3d (  1.0,  1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d (  0.525731112119, -0.850650808354,  0.0 ) ; glVertex3d (  1.61803398875, -0.61803398875,  0.0 ) ; glVertex3d (  1.0, -1.0,  1.0 ) ; glVertex3d ( 0.0, -1.61803398875,  0.61803398875 ) ; glVertex3d ( 0.0, -1.61803398875, -0.61803398875 ) ; glVertex3d (  1.0, -1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d ( -0.525731112119,  0.850650808354,  0.0 ) ; glVertex3d ( -1.61803398875,  0.61803398875,  0.0 ) ; glVertex3d ( -1.0,  1.0,  1.0 ) ; glVertex3d ( 0.0,  1.61803398875,  0.61803398875 ) ; glVertex3d ( 0.0,  1.61803398875, -0.61803398875 ) ; glVertex3d ( -1.0,  1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_LINE_LOOP ) ;
  glNormal3d ( -0.525731112119, -0.850650808354,  0.0 ) ; glVertex3d ( -1.61803398875, -0.61803398875,  0.0 ) ; glVertex3d ( -1.0, -1.0, -1.0 ) ; glVertex3d ( 0.0, -1.61803398875, -0.61803398875 ) ; glVertex3d ( 0.0, -1.61803398875,  0.61803398875 ) ; glVertex3d ( -1.0, -1.0,  1.0 ) ;
  glEnd () ;
}

/*
 *
 */
void FGAPIENTRY glutSolidDodecahedron( void )
{
  FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolidDodecahedron" );

  /* Magic Numbers:  It is possible to create a dodecahedron by attaching two pentagons to each face of
   * of a cube.  The coordinates of the points are:
   *   (+-x,0, z); (+-1, 1, 1); (0, z, x )
   * where x = (-1 + sqrt(5))/2, z = (1 + sqrt(5))/2 or
   *       x = 0.61803398875 and z = 1.61803398875.
   */
  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.0,  0.525731112119,  0.850650808354 ) ; glVertex3d (  0.0,  1.61803398875,  0.61803398875 ) ; glVertex3d ( -1.0,  1.0,  1.0 ) ; glVertex3d ( -0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d (  0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d (  1.0,  1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.0,  0.525731112119, -0.850650808354 ) ; glVertex3d (  0.0,  1.61803398875, -0.61803398875 ) ; glVertex3d (  1.0,  1.0, -1.0 ) ; glVertex3d (  0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d ( -0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d ( -1.0,  1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.0, -0.525731112119,  0.850650808354 ) ; glVertex3d (  0.0, -1.61803398875,  0.61803398875 ) ; glVertex3d (  1.0, -1.0,  1.0 ) ; glVertex3d (  0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d ( -0.61803398875, 0.0,  1.61803398875 ) ; glVertex3d ( -1.0, -1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.0, -0.525731112119, -0.850650808354 ) ; glVertex3d (  0.0, -1.61803398875, -0.61803398875 ) ; glVertex3d ( -1.0, -1.0, -1.0 ) ; glVertex3d ( -0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d (  0.61803398875, 0.0, -1.61803398875 ) ; glVertex3d (  1.0, -1.0, -1.0 ) ;
  glEnd () ;

  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.850650808354,  0.0,  0.525731112119 ) ; glVertex3d (  0.61803398875,  0.0,  1.61803398875 ) ; glVertex3d (  1.0, -1.0,  1.0 ) ; glVertex3d (  1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d (  1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d (  1.0,  1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d ( -0.850650808354,  0.0,  0.525731112119 ) ; glVertex3d ( -0.61803398875,  0.0,  1.61803398875 ) ; glVertex3d ( -1.0,  1.0,  1.0 ) ; glVertex3d ( -1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d ( -1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d ( -1.0, -1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.850650808354,  0.0, -0.525731112119 ) ; glVertex3d (  0.61803398875,  0.0, -1.61803398875 ) ; glVertex3d (  1.0,  1.0, -1.0 ) ; glVertex3d (  1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d (  1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d (  1.0, -1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d ( -0.850650808354,  0.0, -0.525731112119 ) ; glVertex3d ( -0.61803398875,  0.0, -1.61803398875 ) ; glVertex3d ( -1.0, -1.0, -1.0 ) ; glVertex3d ( -1.61803398875, -0.61803398875, 0.0 ) ; glVertex3d ( -1.61803398875,  0.61803398875, 0.0 ) ; glVertex3d ( -1.0,  1.0, -1.0 ) ;
  glEnd () ;

  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.525731112119,  0.850650808354,  0.0 ) ; glVertex3d (  1.61803398875,  0.61803398875,  0.0 ) ; glVertex3d (  1.0,  1.0, -1.0 ) ; glVertex3d ( 0.0,  1.61803398875, -0.61803398875 ) ; glVertex3d ( 0.0,  1.61803398875,  0.61803398875 ) ; glVertex3d (  1.0,  1.0,  1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d (  0.525731112119, -0.850650808354,  0.0 ) ; glVertex3d (  1.61803398875, -0.61803398875,  0.0 ) ; glVertex3d (  1.0, -1.0,  1.0 ) ; glVertex3d ( 0.0, -1.61803398875,  0.61803398875 ) ; glVertex3d ( 0.0, -1.61803398875, -0.61803398875 ) ; glVertex3d (  1.0, -1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d ( -0.525731112119,  0.850650808354,  0.0 ) ; glVertex3d ( -1.61803398875,  0.61803398875,  0.0 ) ; glVertex3d ( -1.0,  1.0,  1.0 ) ; glVertex3d ( 0.0,  1.61803398875,  0.61803398875 ) ; glVertex3d ( 0.0,  1.61803398875, -0.61803398875 ) ; glVertex3d ( -1.0,  1.0, -1.0 ) ;
  glEnd () ;
  glBegin ( GL_POLYGON ) ;
  glNormal3d ( -0.525731112119, -0.850650808354,  0.0 ) ; glVertex3d ( -1.61803398875, -0.61803398875,  0.0 ) ; glVertex3d ( -1.0, -1.0, -1.0 ) ; glVertex3d ( 0.0, -1.61803398875, -0.61803398875 ) ; glVertex3d ( 0.0, -1.61803398875,  0.61803398875 ) ; glVertex3d ( -1.0, -1.0,  1.0 ) ;
  glEnd () ;
}



/* -- INTERFACE FUNCTIONS -------------------------------------------------- */
/* Macro to generate interface functions */
#define DECLARE_SHAPE_INTERFACE(nameICaps)\
    void FGAPIENTRY glutWire##nameICaps( void )\
    {\
        FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWire"#nameICaps );\
        fgh##nameICaps( TRUE );\
    }\
    void FGAPIENTRY glutSolid##nameICaps( void )\
    {\
        FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolid"#nameICaps );\
        fgh##nameICaps( FALSE );\
    }

void FGAPIENTRY glutWireCube( GLdouble dSize )
{
    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWireCube" );
    fghCube( dSize, TRUE );
}
void FGAPIENTRY glutSolidCube( GLdouble dSize )
{
    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolidCube" );
    fghCube( dSize, FALSE );
}

DECLARE_SHAPE_INTERFACE(Icosahedron);
DECLARE_SHAPE_INTERFACE(Octahedron);
DECLARE_SHAPE_INTERFACE(RhombicDodecahedron);

void FGAPIENTRY glutWireSierpinskiSponge ( int num_levels, GLdouble offset[3], GLdouble scale )
{
    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutWireSierpinskiSponge" );
    fghSierpinskiSponge ( num_levels, offset, scale, TRUE );
}
void FGAPIENTRY glutSolidSierpinskiSponge ( int num_levels, GLdouble offset[3], GLdouble scale )
{
    FREEGLUT_EXIT_IF_NOT_INITIALISED ( "glutSolidSierpinskiSponge" );
    fghSierpinskiSponge ( num_levels, offset, scale, FALSE );
}

DECLARE_SHAPE_INTERFACE(Tetrahedron);


/*** END OF FILE ***/
