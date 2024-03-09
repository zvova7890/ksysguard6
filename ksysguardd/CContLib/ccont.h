/*
    Lightweight C Container Library
   
	Copyright (c) 2002 Tobias Koenig <tokoe@kde.org>

	Original code was written by Chris Schlaeger <cs@kde.org>
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#ifndef _CCONT_H
#define _CCONT_H

#ifndef NIL
#define NIL ((void*) 0)
#endif

#define destr_ctnr(x, y) {zero_destr_ctnr(x, y); x=0;}

struct container {
	struct container* next;
	struct container* prev;
	void* data;
};

typedef struct container T_CONTAINER;
typedef struct container* CONTAINER;

typedef long INDEX;

typedef void (*DESTR_FUNC)(void*);
typedef int  (*COMPARE_FUNC)(void*, void*);

/**
 * Initialize the container @p ctnr.
 */
CONTAINER new_ctnr(void);

/**
 * Remove all entries from @p ctnr and reset its
 * internal structure. Use @ref new_ctnr() @em before @em
 * accessing the container again.
 *
 * Note: use the 'destr_ctnr' macro to get zeroed pointer
 *       automatically.
 *
 * @param destr_func The function that is called to
 *                   free the single entries.
 */
void zero_destr_ctnr(CONTAINER ctnr, DESTR_FUNC destr_func);

/**
 * Remove all entries from @p ctnr. There is no need to use
 * @ref new_ctnr() for subsequent access to the container.
 *
 */
void empty_ctnr(CONTAINER rootNode);

/**
 * @return the number of entries in @p ctnr.
 */
INDEX level_ctnr(CONTAINER ctnr);

/**
 * Insert a new entry in container.
 *
 * @param object  A pointer to the object.
 * @param pos     The position where the object should be insert.
 */
void insert_ctnr(CONTAINER ctnr, void* object, INDEX pos);

/**
 * Add a new entry at the end of container.
 *
 * @param object  The object that should be added.
 */
void push_ctnr(CONTAINER ctnr, void* object);

/**
 * Remove an entry from container.
 *
 * @param pos  The position of the object that should be removed.
 *
 * @return A pointer to the removed object or @p 0L if it doesn't exist.
 */
void* remove_at_ctnr(CONTAINER ctnr, INDEX pos);

/**
 * Remove the first entry of container.
 *
 * @return A pointer to the removed object or @p 0L if there is now entry.
 */
void* pop_ctnr(CONTAINER ctnr);

/**
 * @return A pointer to the object at position @a pos
 *         or @p 0L if it doesn't exist.
 */
void* get_ctnr(CONTAINER ctnr, INDEX pos);

/**
 * @return The position of a matching entry.
 *
 * @param compare_func A Pointer to the function that is
 *                     called to compare all entries in the
 *                     container with the given pattern.
 * @param pattern      The pattern for comparison.
 */
INDEX search_ctnr(CONTAINER ctnr, COMPARE_FUNC compare_func, void* pattern);

/**
 * Swap two objects in container.
 *
 * @param pos1 Position of the first object.
 * @param pos2 Position of the second object.
 */
void swap_ctnr(CONTAINER ctnr, INDEX pos1, INDEX pos2);

/**
 * Sort all entries of container.
 *
 * @param compare_func A Pointer to the function that is
 *                     called to compare to entries of the
 *                     container.
 */
void bsort_ctnr(CONTAINER ctnr, COMPARE_FUNC compare_func);

/**
 * Use this function to iterate over the container.
 *
 * for (ptr = first_ctnr(ctnr); ptr; ptr = next_ctnr(ctnr)) {
 * 	do_anything(ptr);
 * }
 *
 * @return A Pointer to the first object in container.
 */
void* first_ctnr(CONTAINER ctnr);

/**
 * Use this function to iterate over the container.
 *
 * @return A Pointer to the next object in container.
 */
void* next_ctnr(CONTAINER ctnr);

/**
 * Use this function to remove the current entry while
 * iterating over the container.
 *
 * @return A Pointer to the removed object or @p 0L if it doesn't exist.
 */
void* remove_ctnr(CONTAINER ctnr);

#endif
