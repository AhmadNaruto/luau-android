#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>
#include "lexbor/html/html.h"
#include "lexbor/html/serialize.h"
#include "lexbor/dom/interfaces/element.h"
#include "lexbor/dom/interfaces/node.h"

// ─── Document Handle Structure ────────────────────────────────────────────────

typedef struct {
    int refcount;
    lxb_html_document_t *lexbor_doc;
} lexsoup_doc_handle_t;

struct lexbor_serialize_ctx_t {
    std::string str;
};

static lxb_status_t lexbor_serialize_callback(const lxb_char_t *data, size_t len, void *ctx) {
    lexbor_serialize_ctx_t *s = (lexbor_serialize_ctx_t*)ctx;
    s->str.append((const char*)data, len);
    return LXB_STATUS_OK;
}

static std::string serialize_node(lxb_dom_node_t *node) {
    if (!node) return "";
    lexbor_serialize_ctx_t s;
    lxb_html_serialize_deep_cb(node, lexbor_serialize_callback, &s);
    return s.str;
}

// ─── Selector Matching Logic ──────────────────────────────────────────────────

static bool match_compound_token(lxb_dom_element_t *el, const std::string &token) {
    if (!el || token.empty() || token == "*") return true;

    size_t not_pos = token.find(":not(");
    if (not_pos != std::string::npos) {
        size_t close_pos = token.find(')', not_pos);
        if (close_pos != std::string::npos) {
            std::string main_part = token.substr(0, not_pos);
            std::string not_part = token.substr(not_pos + 5, close_pos - (not_pos + 5));
            if (!main_part.empty() && !match_compound_token(el, main_part)) return false;
            if (match_compound_token(el, not_part)) return false;
            return true;
        }
    }

    size_t i = 0;
    size_t len = token.length();

    if (token[0] != '.' && token[0] != '#' && token[0] != '[') {
        size_t end_tag = token.find_first_of(".#[", 0);
        std::string tag_name = (end_tag == std::string::npos) ? token : token.substr(0, end_tag);
        size_t vlen = 0;
        const lxb_char_t *el_tag = lxb_dom_element_qualified_name(el, &vlen);
        if (!el_tag) return false;
        std::string el_tag_str((const char*)el_tag, vlen);
        if (strcasecmp(el_tag_str.c_str(), tag_name.c_str()) != 0) return false;
        i = (end_tag == std::string::npos) ? len : end_tag;
    }

    while (i < len) {
        if (token[i] == '.') {
            i++;
            size_t start = i;
            while (i < len && token[i] != '.' && token[i] != '#' && token[i] != '[') i++;
            std::string cls_name = token.substr(start, i - start);
            
            size_t vlen = 0;
            const lxb_char_t *cls_attr = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"class", 5, &vlen);
            if (!cls_attr || vlen == 0) return false;
            
            std::string class_str((const char*)cls_attr, vlen);
            bool found_class = false;
            size_t ci = 0;
            while (ci < vlen) {
                while (ci < vlen && class_str[ci] == ' ') ci++;
                size_t cstart = ci;
                while (ci < vlen && class_str[ci] != ' ') ci++;
                size_t clen = ci - cstart;
                if (clen == cls_name.length() && memcmp(class_str.c_str() + cstart, cls_name.c_str(), clen) == 0) {
                    found_class = true;
                    break;
                }
            }
            if (!found_class) return false;
        } else if (token[i] == '#') {
            i++;
            size_t start = i;
            while (i < len && token[i] != '.' && token[i] != '#' && token[i] != '[') i++;
            std::string id_val = token.substr(start, i - start);
            
            size_t vlen = 0;
            const lxb_char_t *id_attr = lxb_dom_element_get_attribute(el, (const lxb_char_t*)"id", 2, &vlen);
            if (!id_attr || vlen != id_val.length() || memcmp(id_attr, id_val.c_str(), vlen) != 0) return false;
        } else if (token[i] == '[') {
            i++;
            size_t close_bracket = token.find(']', i);
            if (close_bracket == std::string::npos) return false;
            std::string attr_expr = token.substr(i, close_bracket - i);
            i = close_bracket + 1;
            
            size_t eq_pos = attr_expr.find('=');
            if (eq_pos == std::string::npos) {
                size_t vlen = 0;
                const lxb_char_t *val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_expr.c_str(), attr_expr.length(), &vlen);
                if (!val) return false;
            } else {
                char op = '=';
                std::string attr_name = attr_expr.substr(0, eq_pos);
                if (eq_pos > 0 && (attr_expr[eq_pos - 1] == '^' || attr_expr[eq_pos - 1] == '$' || attr_expr[eq_pos - 1] == '*')) {
                    op = attr_expr[eq_pos - 1];
                    attr_name = attr_expr.substr(0, eq_pos - 1);
                }
                std::string expected_val = attr_expr.substr(eq_pos + 1);
                if (!expected_val.empty() && (expected_val.front() == '"' || expected_val.front() == '\'')) expected_val.erase(0, 1);
                if (!expected_val.empty() && (expected_val.back() == '"' || expected_val.back() == '\'')) expected_val.pop_back();

                size_t vlen = 0;
                const lxb_char_t *val = lxb_dom_element_get_attribute(el, (const lxb_char_t*)attr_name.c_str(), attr_name.length(), &vlen);
                if (!val) return false;
                std::string actual_val((const char*)val, vlen);

                if (op == '=') {
                    if (actual_val != expected_val) return false;
                } else if (op == '^') {
                    if (actual_val.rfind(expected_val, 0) != 0) return false;
                } else if (op == '$') {
                    if (actual_val.length() < expected_val.length() || actual_val.compare(actual_val.length() - expected_val.length(), expected_val.length(), expected_val) != 0) return false;
                } else if (op == '*') {
                    if (actual_val.find(expected_val) == std::string::npos) return false;
                }
            }
        } else {
            i++;
        }
    }
    return true;
}

struct selector_token_t {
    std::string token;
    char combinator;
};

static std::vector<selector_token_t> parse_selector_chain(const std::string &selector) {
    std::vector<selector_token_t> chain;
    size_t i = 0;
    size_t len = selector.length();
    char current_comb = ' ';

    while (i < len) {
        while (i < len && (selector[i] == ' ' || selector[i] == '\t')) i++;
        if (i >= len) break;

        if (selector[i] == '>') {
            current_comb = '>';
            i++;
            while (i < len && (selector[i] == ' ' || selector[i] == '\t')) i++;
        }

        size_t start = i;
        while (i < len && selector[i] != ' ' && selector[i] != '\t' && selector[i] != '>') {
            if (selector[i] == '[') {
                while (i < len && selector[i] != ']') i++;
                if (i < len) i++;
            } else {
                i++;
            }
        }

        std::string tok = selector.substr(start, i - start);
        if (!tok.empty()) {
            chain.push_back({tok, current_comb});
            current_comb = ' ';
        }
    }
    return chain;
}

static bool match_node_chain(lxb_dom_node_t *node, const std::vector<selector_token_t> &chain) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT || chain.empty()) return false;

    int idx = (int)chain.size() - 1;
    if (!match_compound_token((lxb_dom_element_t*)node, chain[idx].token)) {
        return false;
    }

    lxb_dom_node_t *curr_node = node;
    idx--;

    while (idx >= 0) {
        char comb = chain[idx + 1].combinator;
        if (comb == '>') {
            lxb_dom_node_t *parent = curr_node->parent;
            if (!parent || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) return false;
            if (!match_compound_token((lxb_dom_element_t*)parent, chain[idx].token)) return false;
            curr_node = parent;
        } else {
            lxb_dom_node_t *parent = curr_node->parent;
            bool found_ancestor = false;
            while (parent) {
                if (parent->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                    if (match_compound_token((lxb_dom_element_t*)parent, chain[idx].token)) {
                        curr_node = parent;
                        found_ancestor = true;
                        break;
                    }
                }
                parent = parent->parent;
            }
            if (!found_ancestor) return false;
        }
        idx--;
    }
    return true;
}

static bool match_node(lxb_dom_node_t *node, const char *selector) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT || !selector || !*selector) return false;

    std::string sel_str(selector);
    size_t start = 0;
    while (start < sel_str.length()) {
        size_t comma = sel_str.find(',', start);
        std::string sub_sel = (comma == std::string::npos) ? sel_str.substr(start) : sel_str.substr(start, comma - start);
        
        size_t s = sub_sel.find_first_not_of(" \t");
        size_t e = sub_sel.find_last_not_of(" \t");
        if (s != std::string::npos && e != std::string::npos) {
            sub_sel = sub_sel.substr(s, e - s + 1);
            auto chain = parse_selector_chain(sub_sel);
            if (match_node_chain(node, chain)) return true;
        }

        if (comma == std::string::npos) break;
        start = comma + 1;
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

JNIEXPORT jstring JNICALL
Java_com_luau_android_lexsoup_LexSoupBridge_nativeOuterHtml(JNIEnv *env, jclass clazz, jlong node_ptr) {
    if (!node_ptr) return env->NewStringUTF("");
    lxb_dom_node_t *node = (lxb_dom_node_t*)(uintptr_t)node_ptr;
    std::string outer = serialize_node(node);
    return env->NewStringUTF(outer.c_str());
}

} // extern "C"
