/*
 * Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 */
#include "config.h"

#include "FrameLoaderClientJava.h"

#include "NotImplemented.h"
#include "AuthenticationChallenge.h"
#include "WebPage.h"
#include "FrameNetworkingContextJava.h"

#include "Chrome.h"
#include "DocumentLoader.h"
#include "DNS.h"
#include "FormState.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "MainFrame.h"
#include "HistoryItem.h"
#include "HTMLFormElement.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PolicyChecker.h"
#include "ProgressTracker.h"
#include "ScriptController.h"
#include "Settings.h"
#include "WindowFeatures.h"

#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JavaScript.h>

#include "com_sun_webkit_LoadListenerClient.h"


namespace WebCore {

namespace {

static JGClass webPageClass;
static JGClass networkContextClass;

static jmethodID setRequestURLMID;
static jmethodID removeRequestURLMID;

static jmethodID fireLoadEventMID;
static jmethodID fireResourceLoadEventMID;
static jmethodID canHandleURLMID;

static jmethodID permitNavigateActionMID;
static jmethodID permitRedirectActionMID;
static jmethodID permitAcceptResourceActionMID;
static jmethodID permitSubmitDataActionMID;
static jmethodID permitEnableScriptsActionMID;
static jmethodID permitNewWindowActionMID;

static jmethodID didClearWindowObjectMID;

static jmethodID frameCreatedMID;
static jmethodID frameDestroyedMID;

static void initRefs(JNIEnv* env)
{
    if (!webPageClass) {
        webPageClass = JLClass(env->FindClass(
            "com/sun/webkit/WebPage"));
        ASSERT(webPageClass);

        setRequestURLMID = env->GetMethodID(webPageClass, "fwkSetRequestURL", "(JILjava/lang/String;)V");
        ASSERT(setRequestURLMID);
        removeRequestURLMID = env->GetMethodID(webPageClass, "fwkRemoveRequestURL", "(JI)V");
        ASSERT(removeRequestURLMID);

        fireLoadEventMID = env->GetMethodID(webPageClass, "fwkFireLoadEvent",
                                            "(JILjava/lang/String;Ljava/lang/String;DI)V");
        ASSERT(fireLoadEventMID);
        fireResourceLoadEventMID = env->GetMethodID(webPageClass, "fwkFireResourceLoadEvent",
                                                    "(JIILjava/lang/String;DI)V");
        ASSERT(fireResourceLoadEventMID);

        permitNavigateActionMID = env->GetMethodID(webPageClass, "fwkPermitNavigateAction",
                                                   "(JLjava/lang/String;)Z");
        ASSERT(permitNavigateActionMID);

        permitRedirectActionMID = env->GetMethodID(webPageClass, "fwkPermitRedirectAction",
                                                   "(JLjava/lang/String;)Z");
        ASSERT(permitRedirectActionMID);

        permitAcceptResourceActionMID = env->GetMethodID(webPageClass, "fwkPermitAcceptResourceAction",
                                                         "(JLjava/lang/String;)Z");
        ASSERT(permitAcceptResourceActionMID);

        permitSubmitDataActionMID = env->GetMethodID(webPageClass, "fwkPermitSubmitDataAction",
                                                     "(JLjava/lang/String;Ljava/lang/String;Z)Z");
        ASSERT(permitSubmitDataActionMID);

        permitEnableScriptsActionMID = env->GetMethodID(webPageClass, "fwkPermitEnableScriptsAction",
                                                        "(JLjava/lang/String;)Z");
        ASSERT(permitEnableScriptsActionMID);

        permitNewWindowActionMID = env->GetMethodID(webPageClass, "fwkPermitNewWindowAction",
                                                    "(JLjava/lang/String;)Z");
        ASSERT(permitNewWindowActionMID);

        didClearWindowObjectMID = env->GetMethodID(webPageClass, "fwkDidClearWindowObject", "(JJ)V");
        ASSERT(didClearWindowObjectMID);

        frameCreatedMID = env->GetMethodID(webPageClass, "fwkFrameCreated", "(J)V");
        ASSERT(frameCreatedMID);

        frameDestroyedMID = env->GetMethodID(webPageClass, "fwkFrameDestroyed", "(J)V");
        ASSERT(frameDestroyedMID);
    }
    if (!networkContextClass) {
        networkContextClass = JLClass(env->FindClass("com/sun/webkit/network/NetworkContext"));
        ASSERT(networkContextClass);

        canHandleURLMID = env->GetStaticMethodID(networkContextClass, "canHandleURL", "(Ljava/lang/String;)Z");
        ASSERT(canHandleURLMID);
    }
}
// This was copied from file "WebKit/Source/WebKit/mac/Misc/WebKitErrors.h".
enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort =                        103,
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
    WebKitErrorPluginWillHandleLoad =                           203
};

enum ContentDispositionType {
    ContentDispositionNone,
    ContentDispositionInline,
    ContentDispositionAttachment,
    ContentDispositionOther
};

// Below function was removed after https://bugs.webkit.org/show_bug.cgi?id=163095
// from HTTPParser.h.
ContentDispositionType contentDispositionType(const String& contentDisposition)
{
    if (contentDisposition.isEmpty())
        return ContentDispositionNone;

    Vector<String> parameters;
    contentDisposition.split(';', parameters);

    String dispositionType = parameters[0];
    dispositionType.stripWhiteSpace();

    if (equalLettersIgnoringASCIICase(dispositionType, "inline"))
        return ContentDispositionInline;

    // Some broken sites just send bogus headers like
    //
    //   Content-Disposition: ; filename="file"
    //   Content-Disposition: filename="file"
    //   Content-Disposition: name="file"
    //
    // without a disposition token... screen those out.
    if (!isValidHTTPToken(dispositionType))
        return ContentDispositionNone;

    // We have a content-disposition of "attachment" or unknown.
    // RFC 2183, section 2.8 says that an unknown disposition
    // value should be treated as "attachment"
    return ContentDispositionAttachment;
}
} // namespace

FrameLoaderClientJava::FrameLoaderClientJava(const JLObject &webPage)
    : m_page(nullptr)
    , m_frame(nullptr)
    , m_isPageRedirected(false)
    , m_hasRepresentation(false)
    , m_webPage(webPage)
{
}

void FrameLoaderClientJava::dispatchDidNavigateWithinPage()
{
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_PAGE_REPLACED,
                  frame()->document()->url(),
                  frame()->loader().documentLoader()->responseMIMEType(),
                  1.0 /* progress */);
}

void FrameLoaderClientJava::frameLoaderDestroyed()
{
    WC_GETJAVAENV_CHKRET(env);
    initRefs(env);

    ASSERT(m_webPage);
    ASSERT(m_frame);
    env->CallVoidMethod(m_webPage, frameDestroyedMID, ptr_to_jlong(m_frame));
    CheckAndClearException(env);

    m_page = 0;
    m_frame = 0;

    delete this;
}

Page* FrameLoaderClientJava::page()
{
    if (!m_page) {
        m_page = WebPage::pageFromJObject(m_webPage);
        ASSERT(m_page);
    }
    return m_page;
}

Frame* FrameLoaderClientJava::frame()
{
    return m_frame;
}

void FrameLoaderClientJava::setFrame(Frame* frame)
{
    ASSERT(frame);
    m_frame = frame;
}

void FrameLoaderClientJava::setRequestURL(Frame* f, int identifier, String url)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    JLString urlJavaString(url.toJavaString(env));
    env->CallVoidMethod(m_webPage, setRequestURLMID, ptr_to_jlong(f), identifier, (jstring)urlJavaString);
    CheckAndClearException(env);
}

void FrameLoaderClientJava::removeRequestURL(Frame* f, int identifier)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    env->CallVoidMethod(m_webPage, removeRequestURLMID, ptr_to_jlong(f), identifier);
    CheckAndClearException(env);
}

void FrameLoaderClientJava::postLoadEvent(Frame* f, int state,
                                          String url, String contentType,
                                          double progress, int errorCode)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    JLString urlJavaString(url.toJavaString(env));
    JLString contentTypeJavaString(contentType.toJavaString(env));

    // First, notify SharedBufferManager, so users can get the full source
    // in CONTENT_RECEIVED handler
    if (state == com_sun_webkit_LoadListenerClient_PAGE_STARTED ||
        state == com_sun_webkit_LoadListenerClient_PROGRESS_CHANGED ||
        state == com_sun_webkit_LoadListenerClient_CONTENT_RECEIVED)
    {
        DocumentLoader* dl = f->loader().activeDocumentLoader();
        if (dl && dl->mainResourceData()) {
            dl->mainResourceData()->size(); // TODO-java: recheck
        }
    }

    // Second, send a load event
    env->CallVoidMethod(m_webPage, fireLoadEventMID,
                        ptr_to_jlong(f), state, (jstring)urlJavaString,
                        (jstring)contentTypeJavaString, progress, errorCode);
    CheckAndClearException(env);
}

void FrameLoaderClientJava::postResourceLoadEvent(Frame* f, int state,
                                                  int id, String contentType,
                                                  double progress, int errorCode)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    JLString contentTypeJavaString(contentType.toJavaString(env));
    // notification for resource event listeners
    env->CallVoidMethod(m_webPage, fireResourceLoadEventMID,
                        ptr_to_jlong(f), state, id,
                        (jstring)contentTypeJavaString, progress, errorCode);
    CheckAndClearException(env);
}

String FrameLoaderClientJava::userAgent(const URL&)
{
    return page()->settings().userAgent();
}

void FrameLoaderClientJava::savePlatformDataToCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientJava::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientJava::transitionToCommittedForNewPage()
{
    FloatRect pageRect = frame()->page()->chrome().pageRect();
    Color bkColor(Color::white);
    bool isTransparent = false;
    FrameView* fv = frame()->view();
    if (fv) {
        bkColor = fv->baseBackgroundColor();
        isTransparent = fv->isTransparent();
    }
    frame()->createView(IntRect(pageRect).size(), bkColor, isTransparent);
}

WTF::Ref<WebCore::DocumentLoader> FrameLoaderClientJava::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

void FrameLoaderClientJava::dispatchWillSubmitForm(FormState&, FramePolicyFunction policyFunction)
{
    // FIXME: This is surely too simple
    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction) {
        return;
    }
    policyFunction(PolicyUse);
}

void FrameLoaderClientJava::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    //uta: for m_pluginWidget we need to do something different
    loader->commitData(data, length);
}

void FrameLoaderClientJava::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest&, FramePolicyFunction policyFunction)
{
    PolicyAction action;

    int statusCode = response.httpStatusCode();
    if (statusCode == 204 || statusCode == 205) {
        // The server does not want us to replace the page contents.
        action = PolicyIgnore;
    } else if (contentDispositionType(response.httpHeaderField(HTTPHeaderName::ContentDisposition)) == ContentDispositionAttachment) {
        // The server wants us to download instead of replacing the page contents.
        // Downloading is handled by the embedder, but we still get the initial
        // response so that we can ignore it and clean up properly.
        action = PolicyIgnore;
    } else if (!canShowMIMEType(response.mimeType())) {
        // Make sure that we can actually handle this type internally.
        action = PolicyIgnore;
    } else {
        // OK, we will render this page.
        action = PolicyUse;
    }

    // NOTE: PolicyChangeError will be generated when action is not PolicyUse.
    policyFunction(action);
}

void FrameLoaderClientJava::dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse& response)
{
    m_response = response;

    if (identifier == m_mainResourceRequestID) {
        double progress = page()->progress().estimatedProgress();
        postLoadEvent(frame(),
                      com_sun_webkit_LoadListenerClient_CONTENTTYPE_RECEIVED,
                      response.url().string(),
                      response.mimeType(),
                      progress);
    }
}

void FrameLoaderClientJava::dispatchDecidePolicyForNewWindowAction(const NavigationAction&,
                                                                   const ResourceRequest& req,
                                                                   FormState*,
                                                                   const String&,
                                                                   FramePolicyFunction policyFunction)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction) {
        return;
    }

    JLString urlString(req.url().string().toJavaString(env));
    bool permit = jbool_to_bool(env->CallBooleanMethod(m_webPage, permitNewWindowActionMID,
                                                       ptr_to_jlong(frame()), (jstring)urlString));
    CheckAndClearException(env);

    // FIXME: I think Qt version marshals this to another thread so when we
    // have multi-threaded download, we might need to do the same
    policyFunction(permit ? PolicyUse : PolicyIgnore);
}

void FrameLoaderClientJava::dispatchDecidePolicyForNavigationAction(const NavigationAction& action,
                                                                    const ResourceRequest& req,
                                                                    FormState*,
                                                                    FramePolicyFunction policyFunction)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction) {
        return;
    }

    bool permit = true;

    JLString urlJavaString(req.url().string().toJavaString(env));

    // 1. Submitting/resubmitting data.
    if (action.type() == NavigationType::FormSubmitted ||
        action.type() == NavigationType::FormResubmitted)
    {
        JLString httpMethodString(req.httpMethod().toJavaString(env));
        permit = env->CallBooleanMethod(m_webPage, permitSubmitDataActionMID,
                                        ptr_to_jlong(frame()), (jstring)urlJavaString,
                                        (jstring)httpMethodString,
                                        bool_to_jbool(action.type() == NavigationType::FormSubmitted));
        CheckAndClearException(env);
    // 2. Redirecting page.
    } else if (m_isPageRedirected) {
        permit = env->CallBooleanMethod(m_webPage, permitRedirectActionMID,
                                        ptr_to_jlong(frame()), (jstring)urlJavaString);
        CheckAndClearException(env);
        m_isPageRedirected = false;
    // 3. Loading document.
    } else {
        permit = env->CallBooleanMethod(m_webPage, permitNavigateActionMID,
                                        ptr_to_jlong(frame()), (jstring)urlJavaString);
        CheckAndClearException(env);
    }

    policyFunction(permit ? PolicyUse : PolicyIgnore);
}

RefPtr<Widget> FrameLoaderClientJava::createPlugin(const IntSize& intSize, HTMLPlugInElement& element, const URL& url,
                                            const Vector<String>& paramNames, const Vector<String>& paramValues,
                                            const String& mimeType, bool)
{
    return adoptRef(new PluginWidgetJava(
        m_webPage,
        &element,
        intSize,
        url.string(),
        mimeType,
        paramNames,
        paramValues));
}

RefPtr<Frame> FrameLoaderClientJava::createFrame(const URL& url, const String& name, HTMLFrameOwnerElement& ownerElement,
                                        const String& referrer, bool, int, int)
{
    JNIEnv* env = WebCore_GetJavaEnv();

    FrameLoaderClientJava* frameLoaderClient = new FrameLoaderClientJava(m_webPage);
    RefPtr<Frame> childFrame(Frame::create(page(), &ownerElement, frameLoaderClient));
    frameLoaderClient->setFrame(childFrame.get());

    childFrame->tree().setName(name);
    m_frame->tree().appendChild(*childFrame);

    childFrame->init();

    // gtk: The creation of the frame may have run arbitrary JS that removed it from the page already.
    if (!childFrame->page()) {
        return 0;
    }

    m_frame->loader().loadURLIntoChildFrame(url, referrer, childFrame.get());

    // gtk: The frame's onload handler may have removed it from the document.
    if (!childFrame->tree().parent()) {
        return 0;
    }

    env->CallVoidMethod(m_webPage, frameCreatedMID, ptr_to_jlong(childFrame.get()));
    CheckAndClearException(env);

    return childFrame;
}

void FrameLoaderClientJava::redirectDataToPlugin(Widget&)
{
    /*
    ASSERT(!m_pluginWidget);
    m_pluginWidget = static_cast<PluginWidgetJava*>(pluginWidget);
    */
    notImplemented();
}

RefPtr<Widget> FrameLoaderClientJava::createJavaAppletWidget(const IntSize&, HTMLAppletElement&, const URL&,
                                                      const Vector<String>&, const Vector<String>&)
{
    notImplemented();
    return nullptr;
}

ObjectContentType FrameLoaderClientJava::objectContentType(const URL& url, const String& mimeType)
{
    //copied from FrameLoaderClientEfl.cpp

    // FIXME: once plugin support is enabled, this method needs to correctly handle the 'shouldPreferPlugInsForImages' flag. See
    // WebCore::FrameLoader::defaultObjectContentType() for an example.

    if (url.isEmpty() && mimeType.isEmpty())
        return ObjectContentType::None;

    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
    String type = mimeType;
    if (type.isEmpty())
        type = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.') + 1));

    if (type.isEmpty())
        return ObjectContentType::Frame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return ObjectContentType::Image;

#if 0 // PluginDatabase is disabled until we have Plugin system done.
    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return ObjectContentNetscapePlugin;
#endif

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return ObjectContentType::Frame;

    if (url.protocol() == "about")
        return ObjectContentType::Frame;

    return ObjectContentType::None;
}

String FrameLoaderClientJava::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientJava::setMainFrameDocumentReady(bool)
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClientJava::hasWebView() const
{
    notImplemented();
    return true;
}

void FrameLoaderClientJava::assignIdentifierToInitialRequest(unsigned long, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientJava::willReplaceMultipartContent() {
    notImplemented(); // TODO-java: recheck
}

void FrameLoaderClientJava::didReplaceMultipartContent() {
    notImplemented(); // TODO-java: recheck
}

void FrameLoaderClientJava::updateCachedDocumentLoader(DocumentLoader&) {
    notImplemented(); // TODO-java: recheck
}

void FrameLoaderClientJava::dispatchDidStartProvisionalLoad()
{
    m_mainResourceRequestID = 0;
}

void FrameLoaderClientJava::dispatchWillSendRequest(DocumentLoader* l, unsigned long identifier, ResourceRequest& req, const ResourceResponse& res)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    Frame* f = l->frame();
    if (!f) {
        f = frame();
    }

    double progress = 0.0;
    progress = page()->progress().estimatedProgress();

    if (m_mainResourceRequestID == 0) {
        m_mainResourceRequestID = identifier;
        postLoadEvent(f,
                      com_sun_webkit_LoadListenerClient_PAGE_STARTED,
                      req.url().string(),
                      res.mimeType(),
                      progress);
    } else if (m_mainResourceRequestID == identifier) { // serever-side redirection
        m_isPageRedirected = true;
        postLoadEvent(f,
                      com_sun_webkit_LoadListenerClient_PAGE_REDIRECTED,
                      req.url().string(),
                      res.mimeType(),
                      progress);
    } else {
        // Check resource policy.
        JLString urlJavaString(req.url().string().toJavaString(env));
        bool permit = jbool_to_bool(env->CallBooleanMethod(m_webPage, permitAcceptResourceActionMID,
                                                           ptr_to_jlong(f), (jstring)urlJavaString));
        CheckAndClearException(env);
        if (!permit) {
/*
            req.setURL(NULL); // will cancel loading
*/
            req.setURL(URL());
        } else {
            setRequestURL(f, identifier, req.url().string());
            postResourceLoadEvent(f,
                                  com_sun_webkit_LoadListenerClient_RESOURCE_STARTED,
                                  identifier,
                                  res.mimeType(),
                                  0.0 /* progress */);
        }
    }
}

void FrameLoaderClientJava::dispatchDidFailLoading(DocumentLoader* dl, unsigned long identifier, const ResourceError& error)
{
    Frame* f = dl->frame();
    if (!f) {
        f = frame();
    }
    postResourceLoadEvent(f,
                          com_sun_webkit_LoadListenerClient_RESOURCE_FAILED,
                          identifier,
                          dl->responseMIMEType(),
                          0.0 /* progress */,
                          error.errorCode());
    removeRequestURL(f, identifier);
}

void FrameLoaderClientJava::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    ASSERT(frame());
    if (!frame()) {
        return;
    }
    DocumentLoader* dl = frame()->loader().activeDocumentLoader();
    if (!dl) {
        return;
    }

    double progress = page()->progress().estimatedProgress();
    int state = error.isCancellation()
        ? com_sun_webkit_LoadListenerClient_LOAD_STOPPED
        : com_sun_webkit_LoadListenerClient_LOAD_FAILED;
    postLoadEvent(frame(), state,
                  dl->url().string(),
                  dl->responseMIMEType(),
                  progress,
                  error.errorCode());
}

void FrameLoaderClientJava::dispatchDidFailLoad(const ResourceError& error)
{
    dispatchDidFailProvisionalLoad(error);
}

// client-side redirection
void FrameLoaderClientJava::dispatchWillPerformClientRedirect(const URL&, double, double)
{
}

void FrameLoaderClientJava::dispatchDidReceiveTitle(const StringWithDirection&)
{
    double progress = page()->progress().estimatedProgress();
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_TITLE_RECEIVED,
                  frame()->document()->url(),
                  frame()->loader().documentLoader()->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::dispatchDidReceiveIcon()
{
    // not called without IconDatabase support, so sending the notification
    // from dispatchDidLoadMainResource()
    /*
    Frame* f = page()->mainFrame();
    if (!f->loader() || !f->document()) {
        return;
    }

    double progress = page()->progress()->estimatedProgress();
    postLoadEvent(com_sun_webkit_LoadListenerClient_ICON_RECEIVED,
                  0, // request id
                  f->document()->url(),
                  f->loader()->documentLoader()->responseMIMEType(),
                  progress);
    */
}

void FrameLoaderClientJava::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long, int)
{
    notImplemented();
}

void FrameLoaderClientJava::dispatchDidFinishDocumentLoad()
{
    if (!frame()->isMainFrame()) {
        // send the notification for the main frame only
        return;
    }

    double progress = page()->progress().estimatedProgress();
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_DOCUMENT_AVAILABLE,
                  frame()->document()->url(),
                  frame()->loader().documentLoader()->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::dispatchDidLoadMainResource(DocumentLoader* l)
{
    double progress = page()->progress().estimatedProgress();
    // send ICON_RECEIVED here instead of dispatchDidReceiveIcon(),
    // see comments in the method for details
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_ICON_RECEIVED,
                  frame()->document()->url(),
                  l->responseMIMEType(),
                  progress);
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_CONTENT_RECEIVED,
                  l->responseURL().string(),
                  l->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::dispatchDidFinishLoading(DocumentLoader* l, unsigned long identifier)
{
    postResourceLoadEvent(frame(),
                          com_sun_webkit_LoadListenerClient_RESOURCE_FINISHED,
                          identifier,
                          l->responseMIMEType(),
                          1.0 /* progress */);
    removeRequestURL(frame(), identifier);
}

void FrameLoaderClientJava::dispatchDidFinishLoad()
{
    double progress = page()->progress().estimatedProgress();
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_PAGE_FINISHED,
                  frame()->document()->url(),
                  frame()->loader().documentLoader()->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::finishedLoading(DocumentLoader* dl)
{
    // This is necessary to create an empty document. See bug 634004.
    // However, we only want to do this if makeRepresentation has been called, to
    // match the behavior on the Mac.
    if (m_hasRepresentation)
        dl->writer().setEncoding("", false);
}

void FrameLoaderClientJava::frameLoadCompleted()
{
    notImplemented();
}

void FrameLoaderClientJava::saveViewStateToItem(HistoryItem&)
{
    notImplemented();
}

void FrameLoaderClientJava::restoreViewState()
{
    notImplemented();
}

Frame* FrameLoaderClientJava::dispatchCreatePage(const NavigationAction& action)
{
    struct WindowFeatures features {};
    Page* newPage = frame()->page()->chrome().createWindow(
                        *frame(),
                        FrameLoadRequest(frame()->document()->securityOrigin(), LockHistory::No,
                                            LockBackForwardList::No, ShouldSendReferrer::MaybeSendReferrer,
                                            AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, // TODO-java: check params
                                            ShouldOpenExternalURLsPolicy::ShouldNotAllow),
                        features,
                        action);

    // createWindow can return null (e.g., popup blocker denies the window).
    if (!newPage)
        return 0;

    return &newPage->mainFrame();
}

bool FrameLoaderClientJava::shouldGoToHistoryItem(HistoryItem* item) const
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item != 0;
}

void FrameLoaderClientJava::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClientJava::didRunInsecureContent(SecurityOrigin&, const URL&)
{
    notImplemented();
}

void FrameLoaderClientJava::didDetectXSS(const URL&, bool)
{
    notImplemented();
}

void FrameLoaderClientJava::makeRepresentation(DocumentLoader*)
{
    m_hasRepresentation = true;
}

void FrameLoaderClientJava::forceLayoutForNonHTML() { notImplemented(); }
void FrameLoaderClientJava::setCopiesOnScroll() { notImplemented(); }
void FrameLoaderClientJava::detachedFromParent2() { notImplemented(); }
void FrameLoaderClientJava::detachedFromParent3() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidDispatchOnloadEvents() {notImplemented(); }

void FrameLoaderClientJava::dispatchDidPushStateWithinPage()
{
    dispatchDidNavigateWithinPage();
}

void FrameLoaderClientJava::dispatchDidReplaceStateWithinPage()
{
    dispatchDidNavigateWithinPage();
}

void FrameLoaderClientJava::dispatchDidPopStateWithinPage() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidReceiveServerRedirectForProvisionalLoad() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidCancelClientRedirect() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidChangeLocationWithinPage() { notImplemented(); }
void FrameLoaderClientJava::dispatchWillClose() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidCommitLoad(std::optional<HasInsecureContent>) {
    // TODO: Look at GTK version
    notImplemented();
}
void FrameLoaderClientJava::dispatchShow() { notImplemented(); }
void FrameLoaderClientJava::cancelPolicyCheck() { notImplemented(); }
void FrameLoaderClientJava::revertToProvisionalState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientJava::willChangeTitle(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientJava::didChangeTitle(DocumentLoader *l) { setTitle(l->title(), l->url()); }

bool FrameLoaderClientJava::canHandleRequest(const ResourceRequest& req) const
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    JLString urlJavaString(req.url().string().toJavaString(env));
    jboolean ret = env->CallStaticBooleanMethod(networkContextClass, canHandleURLMID, (jstring)urlJavaString);
    CheckAndClearException(env);

    return jbool_to_bool(ret);
}

bool FrameLoaderClientJava::canShowMIMEType(const String& mimeType) const
{
    //copy from QT implementation
    String type = mimeType.convertToLowercaseWithoutLocale();
    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedMediaMIMEType(type))
        return true;

#if 0 // PluginDatabase is disabled until we have Plugin system done.
    if (m_frame && m_frame->settings().arePluginsEnabled()
        && PluginDatabase::installedPlugins()->isMIMETypeRegistered(type))
        return true;
#endif

    return false;
}

bool FrameLoaderClientJava::canShowMIMETypeAsHTML(const String&) const
{
    notImplemented();
    return false;
}


bool FrameLoaderClientJava::representationExistsForURLScheme(const String&) const {
    notImplemented();
    return false;
}

String FrameLoaderClientJava::generatedMIMETypeForURLScheme(const String&) const {
    notImplemented();
    return String();
}

void FrameLoaderClientJava::provisionalLoadStarted() {
    notImplemented();
}

void FrameLoaderClientJava::didFinishLoad() {
    notImplemented();
}

void FrameLoaderClientJava::prepareForDataSourceReplacement() {
    notImplemented();
}

void FrameLoaderClientJava::setTitle(const StringWithDirection&, const URL&) {
    notImplemented();
}

bool FrameLoaderClientJava::dispatchDidLoadResourceFromMemoryCache(
    DocumentLoader*,
    const ResourceRequest&,
    const ResourceResponse&,
    int)
{
    notImplemented();
    return false;
}

ResourceError FrameLoaderClientJava::cancelledError(const ResourceRequest& request)
{
    return ResourceError("Error", -999, request.url(), "Request cancelled");
}

ResourceError FrameLoaderClientJava::blockedError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorCannotUseRestrictedPort, request.url(),
                         "Request blocked");
}

ResourceError FrameLoaderClientJava::blockedByContentBlockerError(const ResourceRequest& request)
{
    RELEASE_ASSERT_NOT_REACHED(); // Content Blockers are not enabled for WK1.
    return ResourceError("Error", WebKitErrorCannotShowURL, request.url(),
                         "Cannot show URL");
}

ResourceError FrameLoaderClientJava::cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorCannotShowURL, request.url(),
                         "Cannot show URL");
}

ResourceError FrameLoaderClientJava::interruptedForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorFrameLoadInterruptedByPolicyChange,
                         request.url(), "Frame load interrupted by policy change");
}

ResourceError FrameLoaderClientJava::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError("Error", WebKitErrorCannotShowMIMEType, response.url(),
                         "Cannot show mimetype");
}

ResourceError FrameLoaderClientJava::fileDoesNotExistError(const ResourceResponse& response)
{
    return ResourceError("Error", -998 /* ### */, response.url(),
                         "File does not exist");
}

ResourceError FrameLoaderClientJava::pluginWillHandleLoadError(const ResourceResponse& response)
{
    return ResourceError("Error", WebKitErrorPluginWillHandleLoad, response.url(), "Loading is handled by the media engine");
}

bool FrameLoaderClientJava::shouldFallBack(const ResourceError& error) {
    //Font fallback supported by Java Fonts internaly
    return !(error.isCancellation() || (error.errorCode() == WebKitErrorFrameLoadInterruptedByPolicyChange));
}

bool FrameLoaderClientJava::canCachePage() const {
    return true;
}

void FrameLoaderClientJava::didSaveToPageCache() {
}

void FrameLoaderClientJava::didRestoreFromPageCache() {
}

void FrameLoaderClientJava::dispatchUnableToImplementPolicy(const ResourceError&) {
    notImplemented();
}

void FrameLoaderClientJava::dispatchDidBecomeFrameset(bool) {
   notImplemented();
}


void FrameLoaderClientJava::setMainDocumentError(
    DocumentLoader*,
    const ResourceError&)
{
//    if (m_pluginWidget) {
//        m_pluginWidget = 0;
//    }
    notImplemented();
}

void FrameLoaderClientJava::startDownload(const ResourceRequest&, const String&) {
    notImplemented();
}

void FrameLoaderClientJava::updateGlobalHistory() {
    notImplemented();
}

void FrameLoaderClientJava::updateGlobalHistoryRedirectLinks() {
    notImplemented();
}

void FrameLoaderClientJava::dispatchDidClearWindowObjectInWorld(
    DOMWrapperWorld& world)
{
    JNIEnv* env = WebCore_GetJavaEnv();
    initRefs(env);

    if (&world != &mainThreadNormalWorld()) {
        return;
    }

    JSGlobalContextRef context = toGlobalRef(frame()->script().globalObject(
            mainThreadNormalWorld())->globalExec());
    JSObjectRef windowObject = JSContextGetGlobalObject(context);

    env->CallVoidMethod(m_webPage, didClearWindowObjectMID,
            ptr_to_jlong(context), ptr_to_jlong(windowObject));
    CheckAndClearException(env);
}

void FrameLoaderClientJava::registerForIconNotification(bool) {
    //notImplemented();
}

void FrameLoaderClientJava::convertMainResourceLoadToDownload(DocumentLoader*, SessionID, const ResourceRequest&, const ResourceResponse&)
{
    //notImplemented();
}

Ref<FrameNetworkingContext> FrameLoaderClientJava::createNetworkingContext() {
    return FrameNetworkingContextJava::create(frame());
}


bool FrameLoaderClientJava::shouldUseCredentialStorage(
    DocumentLoader*,
    unsigned long)
{
    notImplemented();
    return false;
}

void FrameLoaderClientJava::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge& challenge)
{
    notImplemented();
    // If the ResourceLoadDelegate doesn't exist or fails to handle the call, we tell the ResourceHandle
    // to continue without credential - this is the best approximation of Mac behavior
    challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
}

void FrameLoaderClientJava::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
}

}
