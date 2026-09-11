/*
 * Copyright (C) Michael Larson on 1/6/2022
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * hash_table.c
 * MGL
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <stdint.h>  // For SIZE_MAX and UINT_MAX
#include <limits.h>  // For UINT_MAX fallback

#ifdef __APPLE__
#include <Metal/Metal.h>
#endif

#include "mgl_log.h"
#include "hash_table.h"
#include "glm_context.h"

void initHashTable(HashTable *ptr, GLuint size)
{
    size_t len;

    if (size == 0)
        size = HASH_TABLE_MIN_SIZE;

    len = sizeof(HashObj) * size;

    ptr->current_name = 1;      // 0 is not a valid GL object name
    ptr->size = size;
    ptr->free_names = NULL;
    ptr->free_count = 0;
    ptr->free_capacity = 0;
    ptr->keys = (HashObj *)malloc(len);

    if (!ptr->keys)
    {
        MGL_ERR("MGL: hash table allocation of %zu bytes failed\n", len);
        ptr->size = 0;
        return;
    }

    bzero(ptr->keys, len);
}

GLuint getNewName(HashTable *table)
{
    // Reuse a deleted name if one is going spare. GL only asks that the name
    // is not currently in use, and recycling keeps the table from growing for
    // as long as the app runs.
    while (table->free_count)
    {
        GLuint name = table->free_names[--table->free_count];

        // The app can also create a name by binding it directly, so a name on
        // this list may have come back to life since it was freed.
        if (name < table->size && table->keys[name].data != NULL)
            continue;

        return name;
    }

    // A table that was never initialised starts at 0, and 0 is reserved in GL.
    if (table->current_name == 0)
        table->current_name = 1;

    return table->current_name++;
}

// Remember a name so getNewName can hand it out again. Failing to grow the
// list is harmless -- the name is simply never reused.
static void releaseName(HashTable *table, GLuint name)
{
    if (name == 0)
        return;

    if (table->free_count == table->free_capacity)
    {
        size_t capacity = table->free_capacity ? table->free_capacity * 2 : HASH_TABLE_MIN_SIZE;
        GLuint *names;

        if (capacity > SIZE_MAX / sizeof(GLuint))
            return;

        names = (GLuint *)realloc(table->free_names, capacity * sizeof(GLuint));

        if (!names)
            return;

        table->free_names = names;
        table->free_capacity = capacity;
    }

    table->free_names[table->free_count++] = name;
}

void *searchHashTable(HashTable *table, GLuint name)
{
    assert(table);
    
    if (name >= table->size)
    {
        // a name past the end simply has not been created yet, which is how
        // every is*() and get*() call asks whether an object exists
        MGL_DEBUG("MGL: searchHashTable - name %u not in table of size %zu\n", name, table->size);
        return NULL;
    }

    return table->keys[name].data;
}

void insertHashElement(HashTable *table, GLuint name, void *data)
{
    assert(table);

    if (name < table->size)
    {
        assert(table->keys[name].data == NULL);

        table->keys[name].data = data;

        return;
    }

    // CRITICAL SECURITY FIX: Prevent integer overflow in hash table resizing
    // some calls allow the user to specify a name...
    while(table->size <= name)
    {
        size_t old_size = table->size;

        // doubling can't grow from zero
        if (old_size == 0)
        {
            table->size = (name < HASH_TABLE_MIN_SIZE) ? HASH_TABLE_MIN_SIZE : (size_t)name + 1;

            table->keys = (HashObj *)calloc(table->size, sizeof(HashObj));

            if (!table->keys)
            {
                MGL_ERR("MGL: hash table allocation failed\n");
                table->size = 0;
                return;
            }

            continue;
        }

        // CRITICAL: Check for integer overflow before multiplication
        if (old_size > UINT_MAX / 2) {
            // SECURITY: Hash table size would overflow, use maximum safe size
            MGL_ERR("MGL SECURITY ERROR: Hash table size would overflow, capping at maximum safe size\n");
            // We could return an error here, but for now we'll cap at UINT_MAX to prevent overflow
            // This might limit functionality but prevents critical security vulnerability
            if (old_size == UINT_MAX) {
                break; // Can't grow any further
            }
            table->size = UINT_MAX;
        } else {
            table->size = old_size * 2; // Safe multiplication
        }

        // CRITICAL: Check for overflow in size calculation before malloc
        if (table->size > SIZE_MAX / sizeof(HashObj)) {
            MGL_ERR("MGL SECURITY ERROR: Hash table allocation would overflow size_t, preventing\n");
            // Reset to old size to prevent crash
            table->size = old_size;
            return; // Exit function safely
        }

        table->keys = (HashObj *)realloc(table->keys, table->size * sizeof(HashObj));
        if (!table->keys) {
            // CRITICAL: Handle allocation failure to prevent crash
            MGL_ERR("MGL SECURITY ERROR: Hash table allocation failed\n");
            table->size = old_size; // Restore old size
            return;
        }

        // Initialize new entries
        for (size_t i = old_size; i < table->size; i++) {
            table->keys[i].data = NULL;
        }
    }

    table->keys[name].data = data;
}

void deleteHashElement(HashTable *table, GLuint name)
{
    assert(table);

    if (name >= table->size) {
        // GL says deleting an unknown name is quietly ignored
        MGL_DEBUG("MGL: deleteHashElement - name %u not in table of size %zu\n", name, table->size);
        return;
    }

    void *obj_data = table->keys[name].data;

    // Perform Metal cleanup for different object types
    if (obj_data) {
        extern GLMContext _ctx;

        // Check if this is a shader object
        if (table == &_ctx->state.shader_table) {
            // Shader-specific Metal cleanup
            Shader *shader = (Shader *)obj_data;
            if (shader->mtl_data.function || shader->mtl_data.library) {
                MGL_DEBUG("MGL: Metal cleanup for shader object %u\n", name);
                // In ARC mode, we just need to set the pointers to nil
                // The memory will be automatically released
                shader->mtl_data.function = NULL;
                shader->mtl_data.library = NULL;
            }
        }
        // Check if this is a program object
        else if (table == &_ctx->state.program_table) {
            // Program-specific Metal cleanup
            Program *program = (Program *)obj_data;
            if (program->mtl_data) {
                MGL_DEBUG("MGL: Metal cleanup for program object %u\n", name);
                // In ARC mode, we just need to set the pointer to nil
                // The memory will be automatically released
                program->mtl_data = NULL;
            }
        }
        // Check if this is a texture object
        else if (table == &_ctx->state.texture_table) {
            // Texture-specific Metal cleanup
            Texture *texture = (Texture *)obj_data;
            if (texture->mtl_data) {
                MGL_DEBUG("MGL: Metal cleanup for texture object %u\n", name);
                // In ARC mode, we just need to set the pointer to nil
                // The memory will be automatically released
                texture->mtl_data = NULL;
            }
        }
        // Check if this is a buffer object
        else if (table == &_ctx->state.buffer_table) {
            // Buffer-specific Metal cleanup
            Buffer *buffer = (Buffer *)obj_data;
            if (buffer->data.mtl_data) {
                MGL_DEBUG("MGL: Metal cleanup for buffer object %u\n", name);
                // In ARC mode, we just need to set the pointer to nil
                // The memory will be automatically released
                buffer->data.mtl_data = NULL;
            }
        }
        // anything else owns no Metal object, so there is nothing to release
    }

    table->keys[name].data = NULL;

    releaseName(table, name);
}
