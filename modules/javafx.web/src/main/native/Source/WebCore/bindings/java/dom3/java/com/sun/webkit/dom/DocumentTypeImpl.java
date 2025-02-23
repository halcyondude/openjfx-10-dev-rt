/*
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
*/

package com.sun.webkit.dom;

import org.w3c.dom.DOMException;
import org.w3c.dom.DocumentType;
import org.w3c.dom.NamedNodeMap;

public class DocumentTypeImpl extends NodeImpl implements DocumentType {
    DocumentTypeImpl(long peer) {
        super(peer);
    }

    static DocumentType getImpl(long peer) {
        return (DocumentType)create(peer);
    }


// Attributes
    public String getName() {
        return getNameImpl(getPeer());
    }
    native static String getNameImpl(long peer);

    public NamedNodeMap getEntities() {
        return NamedNodeMapImpl.getImpl(getEntitiesImpl(getPeer()));
    }
    native static long getEntitiesImpl(long peer);

    public NamedNodeMap getNotations() {
        return NamedNodeMapImpl.getImpl(getNotationsImpl(getPeer()));
    }
    native static long getNotationsImpl(long peer);

    public String getPublicId() {
        return getPublicIdImpl(getPeer());
    }
    native static String getPublicIdImpl(long peer);

    public String getSystemId() {
        return getSystemIdImpl(getPeer());
    }
    native static String getSystemIdImpl(long peer);

    public String getInternalSubset() {
        return getInternalSubsetImpl(getPeer());
    }
    native static String getInternalSubsetImpl(long peer);


// Functions
    public void remove() throws DOMException
    {
        removeImpl(getPeer());
    }
    native static void removeImpl(long peer);


}

