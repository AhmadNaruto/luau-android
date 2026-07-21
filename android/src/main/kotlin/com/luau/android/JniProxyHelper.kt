package com.luau.android

import java.lang.reflect.Method
import java.lang.reflect.Field
import java.lang.reflect.Modifier

object JniProxyHelper {
    
    @JvmStatic
    fun importClass(className: String): Class<*> {
        // Support both dot and slash notations
        val normalized = className.replace('/', '.')
        return Class.forName(normalized)
    }

    @JvmStatic
    fun getField(target: Any, name: String): Any? {
        if (target is Map<*, *>) {
            if (target.containsKey(name)) return target[name]
            for ((k, v) in target) {
                if (k?.toString()?.equals(name, ignoreCase = true) == true) return v
            }
            return null
        }
        val clazz = if (target is Class<*>) target else target.javaClass
        val field = getFieldRecursive(clazz, name) ?: throw NoSuchFieldException("Field $name not found on class ${clazz.name}")
        field.isAccessible = true
        return field.get(if (Modifier.isStatic(field.modifiers)) null else target)
    }

    @JvmStatic
    fun setField(target: Any, name: String, value: Any?) {
        if (target is MutableMap<*, *>) {
            @Suppress("UNCHECKED_CAST")
            (target as MutableMap<Any?, Any?>)[name] = value
            return
        }
        val clazz = if (target is Class<*>) target else target.javaClass
        val field = getFieldRecursive(clazz, name) ?: throw NoSuchFieldException("Field $name not found on class ${clazz.name}")
        field.isAccessible = true
        field.set(if (Modifier.isStatic(field.modifiers)) null else target, value)
    }

    private fun getFieldRecursive(clazz: Class<*>, name: String): Field? {
        var c: Class<*>? = clazz
        while (c != null) {
            try {
                return c.getDeclaredField(name)
            } catch (e: NoSuchFieldException) {
                c = c.superclass
            }
        }
        // Try interface fields or public fields
        return try { clazz.getField(name) } catch (e: Exception) { null }
    }

    @JvmStatic
    fun invokeMethod(target: Any, name: String, args: Array<Any?>): Any? {
        val clazz = if (target is Class<*>) target else target.javaClass
        val methods = (clazz.methods + clazz.declaredMethods).distinct()
        
        // Find best match by name and argument count/types
        val candidate = methods.filter { it.name == name && it.parameterTypes.size == args.size }
            .minByOrNull { method ->
                // Basic type compatibility score (lower is better/more specific)
                var score = 0
                for (i in method.parameterTypes.indices) {
                    val expected = method.parameterTypes[i]
                    val actual = args[i]
                    if (actual == null) {
                        if (expected.isPrimitive) score += 1000 // null cannot map to primitive
                    } else {
                        if (!isCompatible(expected, actual.javaClass)) {
                            score += 10000 // Incompatible types
                        }
                    }
                }
                score
            } ?: run {
                if (target is Class<*>) {
                    val instanceField = getFieldRecursive(target, "INSTANCE")
                    if (instanceField != null) {
                        instanceField.isAccessible = true
                        val instanceObj = instanceField.get(null)
                        if (instanceObj != null) {
                            return invokeMethod(instanceObj, name, args)
                        }
                    }
                }
                throw NoSuchMethodException("No method $name with ${args.size} parameters found on class ${clazz.name}")
            }

        candidate.isAccessible = true
        
        // Convert arguments to expected parameter types if needed (e.g. Number to Double/Int/Float)
        val convertedArgs = Array(args.size) { i ->
            val expected = candidate.parameterTypes[i]
            val actual = args[i]
            convertValue(expected, actual)
        }

        val invokeTarget = if (Modifier.isStatic(candidate.modifiers)) null else (if (target is Class<*>) null else target)
        return candidate.invoke(invokeTarget, *convertedArgs)
    }

    private fun isCompatible(expected: Class<*>, actual: Class<*>): Boolean {
        if (expected.isAssignableFrom(actual)) return true
        val wrapper = getWrapperClass(expected)
        if (wrapper != null && wrapper.isAssignableFrom(actual)) return true
        if (Number::class.java.isAssignableFrom(actual) && (expected.isPrimitive || Number::class.java.isAssignableFrom(expected))) return true
        return false
    }

    private fun getWrapperClass(clazz: Class<*>): Class<*>? {
        if (!clazz.isPrimitive) return null
        return when (clazz) {
            Int::class.javaPrimitiveType -> java.lang.Integer::class.java
            Long::class.javaPrimitiveType -> java.lang.Long::class.java
            Double::class.javaPrimitiveType -> java.lang.Double::class.java
            Float::class.javaPrimitiveType -> java.lang.Float::class.java
            Boolean::class.javaPrimitiveType -> java.lang.Boolean::class.java
            Byte::class.javaPrimitiveType -> java.lang.Byte::class.java
            Char::class.javaPrimitiveType -> java.lang.Character::class.java
            Short::class.javaPrimitiveType -> java.lang.Short::class.java
            else -> null
        }
    }

    private fun convertValue(expected: Class<*>, value: Any?): Any? {
        if (value == null) return null
        if (expected.isInstance(value)) return value
        
        // Convert numbers
        if (value is Number) {
            return when (expected) {
                Int::class.java, Int::class.javaPrimitiveType -> value.toInt()
                Long::class.java, Long::class.javaPrimitiveType -> value.toLong()
                Double::class.java, Double::class.javaPrimitiveType -> value.toDouble()
                Float::class.java, Float::class.javaPrimitiveType -> value.toFloat()
                Byte::class.java, Byte::class.javaPrimitiveType -> value.toByte()
                Short::class.java, Short::class.javaPrimitiveType -> value.toShort()
                else -> value
            }
        }
        return value
    }
}
