.. raw:: html

   <table>
   <tr> 
         <th width="20%"><img src="_static/images/epics-diode.png" alt="EPICS Diode logo"/></th>
         <th><h2>EPICS Diode</h2>Unidirectional transport for EPICS</th>
   </tr>
   </table>
   <hr/>

The main purpose of `EPICS Diode` is to implement a unidirectional transfer of EPICS channel value changes,
usually transported using a bidirectional TCP/IP-based protocol `EPICS Channel Access (CA)` and `EPICS pvAccess (PVA)`,
between segmented networks.

This allows placing a unidirectional network communication device (aka diode) that enables the safe, one-way transfer of data between segmented networks.
The diodes effectively eliminate external points of entry to the sending system, preventing intruders and contagious elements from infiltrating the network.

.. toctree::
   :maxdepth: 3
   :caption: Contents:

   building
   requirements
   design
   design_pva
   protocol
   tools
   example
   requirements_traceability_matrix
   qa_proposal
   test_howto

