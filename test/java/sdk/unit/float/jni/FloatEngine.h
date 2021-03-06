/*
 * Copyright (c) 2016, SRCH2
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the SRCH2 nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SRCH2 BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "Attribute.h"

class FloatEngine {
 
  /** Encapsulates the instance language around a Java RefiningFloat object
      used by the particular JVM housing the Java side of this Engine */
  const JNIClass::Attribute<float> refiningFloat;

  /*  The stored value of this Engine */
  float value;

 public:
  /* Warning: this variable make this class thread unsafe, only
     a single thread may access a given instance of this class.
  
     The JVM context of the Java side of this Engine on a particular bridged
     call.
   */
  JNIEnv *env;

  FloatEngine(jmethodID getValue,
      jclass refiningFloatClassPtr, jmethodID refiningConstructor)
    : refiningFloat(this->env, refiningFloatClassPtr,
          getValue, refiningConstructor) {}

  /* returns a Java RefiningFloat instance with value equivalent
     to the float contained this Engine. */
  jobject getRefiningFloat();
 
  /* Stores a new float value in this Engine, equivalent to the value of
     given FloatAttribute. */
  void setFloat(jobject);
};

