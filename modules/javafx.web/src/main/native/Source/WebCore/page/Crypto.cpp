/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "Crypto.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "SubtleCrypto.h"
#include "WebKitSubtleCrypto.h"
#include <runtime/ArrayBufferView.h>
#include <wtf/CryptographicallyRandomNumber.h>

namespace WebCore {

Crypto::Crypto(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
#if ENABLE(SUBTLE_CRYPTO)
    , m_subtle(SubtleCrypto::create(context))
#endif
{
}

Crypto::~Crypto()
{
}

ExceptionOr<void> Crypto::getRandomValues(ArrayBufferView& array)
{
    if (!isInt(array.getType()))
        return Exception { TYPE_MISMATCH_ERR };
    if (array.byteLength() > 65536)
        return Exception { QUOTA_EXCEEDED_ERR };
    cryptographicallyRandomValues(array.baseAddress(), array.byteLength());
    return { };
}

#if ENABLE(SUBTLE_CRYPTO)

SubtleCrypto& Crypto::subtle()
{
    return m_subtle;
}

ExceptionOr<WebKitSubtleCrypto&> Crypto::webkitSubtle()
{
    if (!isMainThread())
        return Exception { NOT_SUPPORTED_ERR };

    if (!m_webkitSubtle) {
        m_webkitSubtle = WebKitSubtleCrypto::create(*downcast<Document>(scriptExecutionContext()));
        scriptExecutionContext()->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, ASCIILiteral("WebKitSubtleCrypto is deprecated. Please use SubtleCrypto instead."));
    }

    return *m_webkitSubtle;
}

#endif

}
