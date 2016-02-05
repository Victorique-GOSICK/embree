// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "bvh.h"
#include "../../common/ray.h"
#include "../../common/stack_item.h"

namespace embree
{
  namespace isa 
  {

    /*! An item on the stack holds the node ID and distance of that node. */
    struct __aligned(16) StackItemMask
    {    
      size_t mask;
      size_t ptr; 
    };

#if defined(__AVX__)
    template<int types, int K>
      class BVHNNodeTraverserKHit
    {
      typedef BVHN<K> BVH;
      typedef typename BVH::NodeRef NodeRef;
      typedef typename BVH::BaseNode BaseNode;


    public:

      static __forceinline void traverseClosestHit(NodeRef& cur,
                                                   unsigned int &m_trav_active,
                                                   const vbool<K> &vmask,
                                                   const vfloat<K>& tNear,
                                                   const vint<K>& tMask,
                                                   StackItemMask*& stackPtr,
                                                   StackItemMask* stackEnd)
      {

        size_t mask = movemask(vmask);
        assert(mask != 0);
        const BaseNode* node = cur.baseNode(types);

        /*! one child is hit, continue with that child */
        const size_t r0 = __bscf(mask);
        cur = node->child(r0);         
        cur.prefetch(types);
        m_trav_active = tMask[r0]; 
        assert(cur != BVH::emptyNode);
        if (unlikely(mask == 0)) return;

        const unsigned int * const tNear_i = (unsigned int*)&tNear;

        /*! two children are hit, push far child, and continue with closer child */
        NodeRef c0 = cur; 
        unsigned int d0 = tNear_i[r0];
        const size_t r1 = __bscf(mask);
        NodeRef c1 = node->child(r1); 
        c1.prefetch(types); 
        unsigned int d1 = tNear_i[r1];

        assert(c0 != BVH::emptyNode);
        assert(c1 != BVH::emptyNode);
        if (likely(mask == 0)) {
          assert(stackPtr < stackEnd);
          if (d0 < d1) { stackPtr->ptr = c1; stackPtr->mask = tMask[r1]; stackPtr++; cur = c0; m_trav_active = tMask[r0]; return; }
          else         { stackPtr->ptr = c0; stackPtr->mask = tMask[r0]; stackPtr++; cur = c1; m_trav_active = tMask[r1]; return; }
        }

        /*! slow path for more than two hits */
        const size_t hits = __popcnt(movemask(vmask));
        const vint<K> dist_i = select(vmask,(asInt(tNear) & 0xfffffff8) | vint<K>( step ),0x7fffffff);
        const vint<K> dist_i_sorted = sortNetwork(dist_i);
        const vint<K> sorted_index = dist_i_sorted & 7;

        size_t i = hits-1;
        for (;;)
        {
          const unsigned int index = sorted_index[i];
          cur = node->child(index);
          m_trav_active = tMask[index];
          cur.prefetch(types);
          if (unlikely(i==0)) break;
          i--;
          assert(cur != BVH::emptyNode);
          assert(stackPtr < stackEnd);
          stackPtr->ptr = cur; 
          stackPtr->mask = m_trav_active;
          stackPtr++;
        }
               
      }

      static __forceinline int traverseAnyHit(NodeRef& cur,
                                              size_t mask,
                                              const vfloat<K>& tNear,
                                              const vint<K>& tMask,
                                              StackItemMask*& stackPtr,
                                              StackItemMask* stackEnd)
      {
        assert(mask != 0);
        const BaseNode* node = cur.baseNode(types);

        /*! one child is hit, continue with that child */
        size_t r = __bscf(mask);
        cur = node->child(r);         
        cur.prefetch(types);

        
        /* simple in order sequence */
        assert(cur != BVH::emptyNode);
        if (likely(mask == 0)) return tMask[r];
        assert(stackPtr < stackEnd);
        stackPtr->ptr  = cur;
        stackPtr->mask = tMask[r];
        stackPtr++;

        for (; ;)
        {
          assert(stackPtr < stackEnd);
          r = __bscf(mask);
          cur = node->child(r);
          cur.prefetch(types);
          assert(cur != BVH::emptyNode);
          if (likely(mask == 0)) return tMask[r];
          assert(stackPtr < stackEnd);
          stackPtr->ptr  = cur;
          stackPtr->mask = tMask[r];
          stackPtr++;
        }
      }

    };
#endif


    /*! BVH ray stream intersector. */
    template<int N, int types, bool robust, typename PrimitiveIntersector>
      class BVHNStreamIntersector
    {
      /* shortcuts for frequently used types */
      typedef typename PrimitiveIntersector::Precalculations Precalculations;
      typedef typename PrimitiveIntersector::Primitive Primitive;
      typedef BVHN<N> BVH;
      typedef typename BVH::NodeRef NodeRef;
      typedef typename BVH::BaseNode BaseNode;
      typedef typename BVH::Node Node;
      typedef typename BVH::NodeMB NodeMB;

      static const size_t stackSizeChunk  = N*BVH::maxDepth+1;
      static const size_t stackSizeSingle = 1+(N-1)*BVH::maxDepth;
      static const size_t MAX_RAYS_PER_OCTANT = 32;
      static const size_t K = N;

      struct RayContext {
        Vec3fa rdir;      //     rdir.w = tnear;
        Vec3fa org_rdir;  // org_rdir.w = tfar;
      };

    public:
      static void intersect(BVH* bvh, Ray **ray, size_t numRays, size_t flags);
      static void occluded (BVH* bvh, Ray **ray, size_t numRays, size_t flags);
    };



  }
}