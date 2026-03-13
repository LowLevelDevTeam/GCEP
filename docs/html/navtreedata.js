/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "My Project", "index.html", [
    [ "Technical Design Document (TDD) for Gaming Engine Campus Paris", "md__r_e_a_d_m_e.html", [
      [ "Document Header", "md__r_e_a_d_m_e.html#autotoc_md1", null ],
      [ "- <b>Date:</b> 2026-02-17", "md__r_e_a_d_m_e.html#autotoc_md2", null ],
      [ "Authors", "md__r_e_a_d_m_e.html#autotoc_md3", null ],
      [ "Revision History", "md__r_e_a_d_m_e.html#autotoc_md4", null ],
      [ "Table of Contents", "md__r_e_a_d_m_e.html#autotoc_md5", null ],
      [ "1. Introduction", "md__r_e_a_d_m_e.html#autotoc_md7", [
        [ "1.1 Purpose", "md__r_e_a_d_m_e.html#autotoc_md8", null ],
        [ "1.2 Scope", "md__r_e_a_d_m_e.html#autotoc_md9", null ],
        [ "1.3 Definitions, Acronyms, and Abbreviations", "md__r_e_a_d_m_e.html#autotoc_md10", null ],
        [ "1.4 References / Dependencies", "md__r_e_a_d_m_e.html#autotoc_md11", null ],
        [ "1.5 Document Overview", "md__r_e_a_d_m_e.html#autotoc_md12", null ]
      ] ],
      [ "2. System Overview", "md__r_e_a_d_m_e.html#autotoc_md14", [
        [ "2.1 High-Level Description", "md__r_e_a_d_m_e.html#autotoc_md15", null ],
        [ "2.2 System Context Diagram", "md__r_e_a_d_m_e.html#autotoc_md16", null ],
        [ "2.3 Major Components", "md__r_e_a_d_m_e.html#autotoc_md17", null ]
      ] ],
      [ "3. Requirements", "md__r_e_a_d_m_e.html#autotoc_md19", [
        [ "3.1 Functional Requirements", "md__r_e_a_d_m_e.html#autotoc_md20", null ],
        [ "3.2 Non-Functional Requirements", "md__r_e_a_d_m_e.html#autotoc_md21", null ],
        [ "3.3 Use Cases", "md__r_e_a_d_m_e.html#autotoc_md22", null ],
        [ "3.4 Design Constraints and Assumptions", "md__r_e_a_d_m_e.html#autotoc_md23", null ]
      ] ],
      [ "4. System Architecture &amp; Design", "md__r_e_a_d_m_e.html#autotoc_md25", [
        [ "4.1 Architectural Overview", "md__r_e_a_d_m_e.html#autotoc_md26", null ],
        [ "4.2 Module Breakdown", "md__r_e_a_d_m_e.html#autotoc_md27", null ],
        [ "4.3 Interaction Diagrams", "md__r_e_a_d_m_e.html#autotoc_md28", [
          [ "Sequence Diagram: Rendering a Frame", "md__r_e_a_d_m_e.html#autotoc_md29", null ],
          [ "Game Loop Flowchart", "md__r_e_a_d_m_e.html#autotoc_md30", null ]
        ] ],
        [ "4.4 Design Decisions and Rationale", "md__r_e_a_d_m_e.html#autotoc_md31", null ]
      ] ],
      [ "5. Detailed Module Design", "md__r_e_a_d_m_e.html#autotoc_md33", [
        [ "5.1 Class Diagrams and Data Structures", "md__r_e_a_d_m_e.html#autotoc_md34", null ],
        [ "5.2 Key Algorithms and Code Snippets", "md__r_e_a_d_m_e.html#autotoc_md35", [
          [ "Basic Rendering Loop in C++", "md__r_e_a_d_m_e.html#autotoc_md36", null ]
        ] ],
        [ "8.2 Integration Testing", "md__r_e_a_d_m_e.html#autotoc_md37", null ],
        [ "8.3 Regression Testing", "md__r_e_a_d_m_e.html#autotoc_md38", null ],
        [ "8.4 Testing Tools and Frameworks", "md__r_e_a_d_m_e.html#autotoc_md39", null ]
      ] ],
      [ "9. Tools, Environment, and Deployment", "md__r_e_a_d_m_e.html#autotoc_md41", [
        [ "9.1 Development Tools and IDEs", "md__r_e_a_d_m_e.html#autotoc_md42", null ],
        [ "9.2 Build System and Automation", "md__r_e_a_d_m_e.html#autotoc_md43", null ],
        [ "9.3 Version Control", "md__r_e_a_d_m_e.html#autotoc_md44", null ],
        [ "9.4 Deployment Environment", "md__r_e_a_d_m_e.html#autotoc_md45", null ]
      ] ],
      [ "10. Project Timeline and Milestones", "md__r_e_a_d_m_e.html#autotoc_md47", null ],
      [ "11. Project choices", "md__r_e_a_d_m_e.html#autotoc_md49", [
        [ "11.1 Graphical APIs", "md__r_e_a_d_m_e.html#autotoc_md50", null ],
        [ "11.2 Audio APIs", "md__r_e_a_d_m_e.html#autotoc_md51", null ],
        [ "11.3 Physics APIs", "md__r_e_a_d_m_e.html#autotoc_md52", null ],
        [ "11.4 Window manager APIs", "md__r_e_a_d_m_e.html#autotoc_md53", null ],
        [ "11.5 GUI library", "md__r_e_a_d_m_e.html#autotoc_md54", null ],
        [ "11.6 Maths library", "md__r_e_a_d_m_e.html#autotoc_md55", null ],
        [ "11.7 Build system", "md__r_e_a_d_m_e.html#autotoc_md56", null ],
        [ "11.8 Conventions", "md__r_e_a_d_m_e.html#autotoc_md57", null ]
      ] ],
      [ "12. Appendices", "md__r_e_a_d_m_e.html#autotoc_md59", [
        [ "12.1 Glossary", "md__r_e_a_d_m_e.html#autotoc_md60", null ],
        [ "12.2 Additional Diagrams", "md__r_e_a_d_m_e.html#autotoc_md61", null ],
        [ "12.3 References and Further Reading", "md__r_e_a_d_m_e.html#autotoc_md62", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"index.html"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';