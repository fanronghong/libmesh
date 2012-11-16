// $Id: cell_tet4.h,v 1.2 2004-01-03 15:37:42 benkirk Exp $

// The libMesh Finite Element Library.
// Copyright (C) 2002-2004  Benjamin S. Kirk, John W. Peterson
  
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
  
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



#ifndef __cell_tet4_h__
#define __cell_tet4_h__

// C++ includes

// Local includes
#include "cell_tet.h"




/**
 * The \p Tet4 is an element in 3D composed of 4 nodes.
 * It is numbered like this:
   \verbatim
  TET4:
        3
        o
       /|\
      / | \
     /  |  \
  0 o...|...o 2
     \  |  /
      \ | /
       \|/
        o
        1
  \endverbatim
 */

// ------------------------------------------------------------
// Tet4 class definition
class Tet4 : public Tet
{
public:

  /**
   * Constructor.  By default this element has no parent.
   */
  Tet4  (const Elem* p=NULL);
  
  /**
   * @returns \p TET4
   */
  ElemType type () const { return TET4; }
  
  /**
   * @returns 4
   */
  unsigned int n_nodes() const { return 4; }

  /**
   * @returns 1
   */
  unsigned int n_sub_elem() const { return 1; }
    
  /**
   * @returns FIRST
   */
  Order default_order() const { return FIRST; }
  
  /**
   * Builds a \p TRI3 built coincident with face i.  
   * The \p AutoPtr<Elem> handles the memory aspect.
   */
  AutoPtr<Elem> build_side (const unsigned int i) const;

  const std::vector<unsigned int> tecplot_connectivity(const unsigned int sc=0) const;
  
    
  void vtk_connectivity(const unsigned int sc,
			std::vector<unsigned int> *conn = NULL) const;
  
  unsigned int vtk_element_type (const unsigned int) const
  { return 10; }
  
 
protected:

  
#ifdef ENABLE_AMR
  
  /**
   * Matrix used to create the elements children.
   */
  float embedding_matrix (const unsigned int i,
			  const unsigned int j,
			  const unsigned int k) const
  { return _embedding_matrix[i][j][k]; }

  /**
   * Matrix that computes new nodal locations/solution values
   * from current nodes/solution.
   */
  static const float _embedding_matrix[8][4][4];
  
#endif
  
};



// ------------------------------------------------------------
// Tet4 class member functions
inline
Tet4::Tet4(const Elem* p) :
  Tet(Tet4::n_nodes(), p) 
{
}



#endif