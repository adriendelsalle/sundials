%CVSensRhsFn - type for user provided sensitivity RHS function.
%
%   The function ODESFUN must be defined as 
%        FUNCTION YSD = ODESFUN(T,Y,YD,YS)
%   and must return a matrix YSD corresponding to fS(t,y,yS).
%   If a user data structure DATA was specified in CVodeMalloc, then
%   ODESFUN must be defined as
%        FUNCTION [YSD, NEW_DATA] = ODESFUN(T,Y,YD,YS,DATA)
%   If the local modifications to the user data structure are needed in
%   other user-provided functions then, besides setting the matrix YSD,
%   the ODESFUN function must also set NEW_DATA. Otherwise, it should
%   set NEW_DATA=[] (do not set NEW_DATA = DATA as it would lead to 
%   unnecessary copying).
%
%   See also CVodeSetOptions
%
%   NOTE: ODESFUN is specified through the property FSARhsFn to CVodeSetOptions 
%   and is used only if the property SensiAnalysis was set to 'FSA' and if the
%   property FSARhsType was set to 'All'.

% Radu Serban <radu@llnl.gov>
% Copyright (c) 2005, The Regents of the University of California.
% $Revision: 1.1 $Date$