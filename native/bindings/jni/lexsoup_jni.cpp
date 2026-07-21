#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include "lexbor/html/html.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"

// ─── Document Handle Structure ────────────────────────────────────────────────

typedef struct {
    int refcount;
    lxb_html_document_t *lexbor_doc;
} lexsoup_doc_handle_t;

// ─── Selector Matching Logic ──────────────────────────────────────────────────

static bool match_single_selector(lxb_dom_element_t *el, const char *sel, size_t sel_len) {
    if (sel_len == 0) return false;
    const char *dot = (const char*)memchr(sel, '.', sel_len);
    if (dot) {
        size_t tag_len = dot - sel;
        if (tag_len > 0) {
            size_t vlen = 0;
            const lxb_char_t *name = lxb_dom_element_qualified_name(el, &vlen);
            if (!name || vlen != tag_len || memcmp(name, sel, tag_len) != 0) {
                return false;
            }
        }
        const char *class_target = dot + 1;
        size_t class_target_len = sel_len - (dot - sel) - 1;

        size_t vlen = 0;
        const lxb_char_t *cls = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"class", 5, &vlen);
        if (!cls || vlen == 0) return false;
        const char *class_str = (const char*)cls;
        size_t ci = 0;
        while (ci < vlen) {
            while (ci < vlen && class_str[ci] == ' ') ci++;
            size_t start = ci;
            while (ci < vlen && class_str[ci] != ' ') ci++;
            size_t wlen = ci - start;
            if (wlen == class_target_len && memcmp(class_str + start, class_target, class_target_len) == 0) {
                return true;
            }
        }
        return false;
    } else if (sel[0] == '#') {
        size_t vlen = 0;
        const lxb_char_t *id_attr = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"id", 2, &vlen);
        if (!id_attr || vlen == 0) return false;
        return (vlen == sel_len - 1 && memcmp(id_attr, sel + 1, vlen) == 0);
    } else if (sel[0] == '[') {
        const char *close_bracket = (const char*)memchr(sel, ']', sel_len);
        if (!close_bracket) return false;
        size_t attr_len = close_bracket - sel - 1;
        const char *attr_name = sel + 1;
        size_t vlen = 0;
        const lxb_char_t *attr_val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_name, attr_len, &vlen);
        return (attr_val != NULL);
    } else {
        const char *bracket = (const char*)memchr(sel, '[', sel_len);
        size_t tag_len = bracket ? (size_t)(bracket - sel) : sel_len;
        size_t vlen = 0;
        const lxb_char_t *name = lxb_dom_element_qualified_name(el, &vlen);
        if (!name || vlen != tag_len || memcmp(name, sel, tag_len) != 0) {
            return false;
        }
        if (bracket) {
            const char *close_bracket = (const char*)memchr(bracket, ']', sel_len - tag_len);
            if (!close_bracket) return false;
            size_t attr_len = close_bracket - bracket - 1;
            const char *attr_name = bracket + 1;
            size_t aval_len = 0;
            const lxb_char_t *attr_val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_name, attr_len, &aval_len);
            return (attr_val != NULL && aval_len > 0);
        }
        return true;
    }
}

static bool match_node(lxb_dom_node_t *node, const char *selector) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) return false;
    lxb_dom_element_t *el = (lxb_dom_element_t*)node;

    const char *last_space = strrchr(selector, ' ');
    if (!last_space) {
        return match_single_selector(el, selector, strlen(selector));
    }

    const char *target_sel = last_space + 1;
    if (*target_sel == '>') target_sel++;
    while (*target_sel == ' ') target_sel++;
    if (*target_sel == '\0') return false;

    if (!match_single_selector(el, target_sel, strlen(target_sel))) {
        return false;
    }

    const char *first_token = selector;
    size_t first_len = last_space - selector;
    while (first_len > 0 && (first_token[first_len - 1] == ' ' || first_token[first_len - 1] == '>')) {
        first_len--;
    }

    lxb_dom_node_t *parent = node->parent;
    while (parent) {
        if (parent->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            if (match_single_selector((lxb_dom_element_t*)parent, first_token, first_len)) {
                return true;
            }
        }
        parent = parent->parent;
    }
    return false;
}

static void find_all_nodes_list(lxb_dom_node_t *node, const char *selector, jlong *out_nodes, size_t max_nodes, size_t *count) {
    if (!node || *count >= max_nodes) return;
    if (match_node(node, selector)) {
        out_nodes[(*count)++] = (jlong)(uintptr_t)node;
    }
    lxb_dom_node_t *child = node->first_child;
    while (child) {
        find_all_nodes_list(child, selector, out_nodes, max_nodes, count);
        child = child->next;
    }
}

static lxb_dom_node_t* find_first_node(lxb_dom_node_t *node, const char *selector) {
    if (!node) return NULL;
    if (match_node(node, selector)) return node;

    lxb_dom_node_t *child = node->first_child;
    while (child) {
        lxb_dom_node_t *found = find_first_node(child, selector);
        if (found) return found;
        child = child->next;
    }
    return NULL;
}

// ─── Text Extraction Helper ───────────────────────────────────────────────────

static void append_node_text(lxb_dom_node_t *node, char **buf, size_t *len, size_t *cap) {
    if (!node) return;
    if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
        size_t text_len = 0;
        const lxb_char_t *text_val = lxb_dom_node_text_content(node, &text_len);
        if (text_val && text_len > 0) {
            size_t needed = *len + text_len + 1;
            if (needed > *cap) {
                size_t new_cap = needed * 2;
                char *new_buf = (char*)realloc(*buf, new_cap);
                if (!new_buf) return;
                *buf = new_buf;
                *cap = new_cap;
            }
            memcpy(*buf + *len, text_val, text_len);
            *len += text_len;
            (*buf)[*len] = '\0';
        }
    }
    lxb_dom_node_t *child = node->first_child;
    while (child) {
        append_node_text(child, buf, len, cap);
        child = child->next;
    }
}

// ─── JNI Native Functions ─────────────────────────────────────────────────────

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeParse(JNIEnv *env, jclass clazz, jstring html_str) {
    if (!html_str) return 0;
    const char *html = env->GetStringUTFChars(html_str, NULL);
    if (!html) return 0;

    lexsoup_doc_handle_t *handle = (lexsoup_doc_handle_t*)malloc(sizeof(lexsoup_doc_handle_t));
    if (!handle) {
        env->ReleaseStringUTFChars(html_str, html);
        return 0;
    }
    handle->refcount = 1;
    handle->lexbor_doc = lxb_html_document_create();
    if (!handle->lexbor_doc) {
        free(handle);
        env->ReleaseStringUTFChars(html_str, html);
        return 0;
    }

    lxb_status_t status = lxb_html_document_parse(handle->lexbor_doc, (const lxb_char_t*)html, strlen(html));
    env->ReleaseStringUTFChars(html_str, html);

    if (status != LXB_STATUS_OK) {
        lxb_html_document_destroy(handle->lexbor_doc);
        free(handle);
        return 0;
    }
    return (jlong)(uintptr_t)handle;
}

JNIEXPORT void JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeFreeDoc(JNIEnv *env, jclass clazz, jlong handle_ptr) {
    if (!handle_ptr) return;
    lexsoup_doc_handle_t *handle = (lexsoup_doc_handle_t*)(uintptr_t)handle_ptr;
    if (handle) {
        if (--handle->refcount <= 0) {
            if (handle->lexbor_doc) {
                lxb_html_document_destroy(handle->lexbor_doc);
            }
            free(handle);
        }
    }
}

JNIEXPORT jlong JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeGetRootNode(JNIEnv *env, jclass clazz, jlong handle_ptr) {
    if (!handle_ptr) return 0;
    lexsoup_doc_handle_t *handle = (lexsoup_doc_handle_t*)(uintptr_t)handle_ptr;
    if (!handle || !handle->lexbor_doc) return 0;
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t*)handle->lexbor_doc);
    return (jlong)(uintptr_t)root;
}

JNIEXPORT jlongArray JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeSelect(JNIEnv *env, jclass clazz, jlong handle_ptr, jlong node_ptr, jstring selector_str) {
    if (!handle_ptr || !selector_str) return env->NewLongArray(0);
    lexsoup_doc_handle_t *handle = (lexsoup_doc_handle_t*)(uintptr_t)handle_ptr;
    if (!handle || !handle->lexbor_doc) return env->NewLongArray(0);

    lxb_dom_node_t *root = (node_ptr != 0) ? (lxb_dom_node_t*)(uintptr_t)node_ptr : lxb_dom_document_root((lxb_dom_document_t*)handle->lexbor_doc);
    if (!root) return env->NewLongArray(0);

    const char *sel = env->GetStringUTFChars(selector_str, NULL);
    if (!sel) return env->NewLongArray(0);

    static const size_t MAX_NODES = 4096;
    jlong *nodes = (jlong*)malloc(sizeof(jlong) * MAX_NODES);
    size_t count = 0;
    if (nodes) {
        find_all_nodes_list(root, sel, nodes, MAX_NODES, &count);
    }
    env->ReleaseStringUTFChars(selector_str, sel);

    jlongArray res = env->NewLongArray(count);
    if (res && count > 0) {
        env->SetLongArrayRegion(res, 0, count, nodes);
    }
    if (nodes) free(nodes);
    return res;
}

JNIEXPORT jlong JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeSelectFirst(JNIEnv *env, jclass clazz, jlong handle_ptr, jlong node_ptr, jstring selector_str) {
    if (!handle_ptr || !selector_str) return 0;
    lexsoup_doc_handle_t *handle = (lexsoup_doc_handle_t*)(uintptr_t)handle_ptr;
    if (!handle || !handle->lexbor_doc) return 0;

    lxb_dom_node_t *root = (node_ptr != 0) ? (lxb_dom_node_t*)(uintptr_t)node_ptr : lxb_dom_document_root((lxb_dom_document_t*)handle->lexbor_doc);
    if (!root) return 0;

    const char *sel = env->GetStringUTFChars(selector_str, NULL);
    if (!sel) return 0;

    lxb_dom_node_t *found = find_first_node(root, sel);
    env->ReleaseStringUTFChars(selector_str, sel);

    return (jlong)(uintptr_t)found;
}

JNIEXPORT jstring JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeAttr(JNIEnv *env, jclass clazz, jlong node_ptr, jstring attr_name_str) {
    if (!node_ptr || !attr_name_str) return env->NewStringUTF("");
    lxb_dom_node_t *node = (lxb_dom_node_t*)(uintptr_t)node_ptr;
    if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) return env->NewStringUTF("");

    const char *attr_name = env->GetStringUTFChars(attr_name_str, NULL);
    if (!attr_name) return env->NewStringUTF("");

    size_t len = 0;
    const lxb_char_t *val = lxb_dom_element_get_attribute((lxb_dom_element_t*)node, (const lxb_char_t*)attr_name, strlen(attr_name), &len);
    env->ReleaseStringUTFChars(attr_name_str, attr_name);

    if (val && len > 0) {
        char *copy = (char*)malloc(len + 1);
        if (!copy) return env->NewStringUTF("");
        memcpy(copy, val, len);
        copy[len] = '\0';
        jstring res = env->NewStringUTF(copy);
        free(copy);
        return res;
    }
    return env->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeText(JNIEnv *env, jclass clazz, jlong node_ptr) {
    if (!node_ptr) return env->NewStringUTF("");
    lxb_dom_node_t *node = (lxb_dom_node_t*)(uintptr_t)node_ptr;

    char *buf = NULL;
    size_t len = 0, cap = 0;
    append_node_text(node, &buf, &len, &cap);

    if (buf && len > 0) {
        jstring res = env->NewStringUTF(buf);
        free(buf);
        return res;
    }
    if (buf) free(buf);
    return env->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeTagName(JNIEnv *env, jclass clazz, jlong node_ptr) {
    if (!node_ptr) return env->NewStringUTF("");
    lxb_dom_node_t *node = (lxb_dom_node_t*)(uintptr_t)node_ptr;
    if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) return env->NewStringUTF("");

    size_t len = 0;
    const lxb_char_t *name = lxb_dom_element_qualified_name((lxb_dom_element_t*)node, &len);
    if (name && len > 0) {
        return env->NewStringUTF((const char*)name);
    }
    return env->NewStringUTF("");
}

} // extern "C"
