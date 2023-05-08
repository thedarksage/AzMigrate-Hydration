//---------------------------------------------------------------
//  <copyright file="PrerequisiteHelper.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Helper routines for Prereqs
//  </summary>
//
//  History:     30-Sep-2014   Ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;

namespace Microsoft.DisasterRecovery.SetupFramework
{
    /// <summary>
    /// PrerequisiteHelper class for evaluating PreRequisites for Setup and Registration.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.OrderingRules",
        "SA1201:ElementsMustAppearInTheCorrectOrder",
        Justification = "Grouping based on usage")]
    public class PrerequisiteHelper
    {
        #region Static members and methods
        
        #region Private Members
        /// <summary>
        /// List of all Prereqs.
        /// </summary>
        private static Dictionary<string, PrerequisiteHelper> preRequisiteList = new Dictionary<string, PrerequisiteHelper>();

        /// <summary>
        /// If all fatal prereqs have passed.
        /// </summary>
        private static bool? preReqPassed = null;
        #endregion

        #region Public Members
        /// <summary>
        /// Prereq evaluation completion event
        /// </summary>
        public static event PrerequisiteEventGenerator.PreReqEventHandler CompletionEvent;
        #endregion

        /// <summary>
        /// Evaluates all the Pending Pre-Reqs (not evaluated earlier) and sets Passed value for each of them.
        /// </summary>
        public static void EvaluateAllPendingPreRequisitesAsync()
        {
            foreach (KeyValuePair<string, PrerequisiteHelper> preReq in preRequisiteList)
            {
                if (!preReq.Value.Passed.HasValue)
                {
                    Trc.Log(LogLevel.Always, "Evaluating PreReq " + preReq.Key);
                    preReq.Value.Evaluate();
                }
            }

            if (CompletionEvent != null)
            {
                CompletionEvent(null, new PrerequisiteEventGenerator());
            }
        }

        /// <summary>
        /// Checks if all fatal Pre-Reqs have passed or not.
        /// </summary>
        /// <returns>True if all fatal Pre Reqs have passed, false otherwise.</returns>
        public static bool StandardPreRequisitesPassed()
        {
            Trc.Log(LogLevel.Debug, "Checking if all fatal prereqs have passed");
            try
            {
                preReqPassed = true;
                foreach (KeyValuePair<string, PrerequisiteHelper> preReq in preRequisiteList)
                {
                    if (preReq.Value.IsFatal && !preReq.Value.Passed.Value)
                    {
                        preReqPassed = false;
                        break;
                    }
                }
            }
            catch (Exception)
            {
                preReqPassed = false;
            }

            return preReqPassed.Value;
        }

        /// <summary>
        /// Updates UI elements with respect to current PrerequisiteHelper object.
        /// </summary>
        public static void UpdateUIElements()
        {
            foreach (KeyValuePair<string, PrerequisiteHelper> preReq in preRequisiteList)
            {
                preReq.Value.associatedHelper.UpdatePreRequisitesInUI(preReq.Value);
            }
        }

        #endregion
        
        #region Public Members
        /// <summary>
        /// Gets or sets Prereq name
        /// </summary>
        public string PreRequisiteName { get; set; }

        /// <summary>
        /// Gets true if prereq has passed, false if failed, null if yet to be evaluated.
        /// </summary>
        public bool? Passed { get; internal set; }

        /// <summary>
        /// Gets a value indicating whether this prereq is critical to continue.
        /// </summary>
        public bool IsFatal { get; internal set; }

        /// <summary>
        /// Gets associated dialog for this prereq
        /// </summary>
        public DialogHelper HelpLink { get; internal set; }
        #endregion

        #region Private Members
        /// <summary>
        /// Evaluation function.
        /// </summary>
        private Prerequisite.ProcessPrerequisite evalFunc;

        /// <summary>
        /// UI handle to edit objects.
        /// </summary>
        private PrerequisiteUIHelper associatedHelper;
        #endregion

        /// <summary>
        /// Initializes a new instance of the PrerequisiteHelper class.
        /// </summary>
        /// <param name="preRequisiteName">String defining Pre Requisite.</param>
        /// <param name="evaluationFunction">Function to be used to evaluate the Pre Req. This function must return bool response.</param>
        /// <param name="helpLink">Help link to be shown to user.</param>
        /// <param name="isFatal">If set to true, this pre req needs to pass for setup/registration to continue.</param>
        private PrerequisiteHelper(
            string preRequisiteName, 
            Prerequisite.ProcessPrerequisite evaluationFunction,
            DialogHelper helpLink,
            bool isFatal)
        {
            this.PreRequisiteName = preRequisiteName;
            this.IsFatal = isFatal;
            this.evalFunc = evaluationFunction;
            this.HelpLink = helpLink;
        }

        /// <summary>
        /// If current PreReq is not defined already then adds it to PrerequisiteHelper list.
        /// </summary>
        /// <param name="preRequisiteName">String defining Pre Requisite.</param>
        /// <param name="evaluationFunction">Function to be used to evaluate the Pre Req. This function must return bool response.</param>
        /// <param name="helpLink">Help link to be shown to user.</param>
        /// <param name="isFatal">If set to true, this pre req needs to pass for setup/registration to continue.</param>
        /// <returns>PrerequisiteHelper object with PreReq details is new PreReq, null otherwise.</returns>
        public static PrerequisiteHelper DefinePreRequisite(
            string preRequisiteName,
            Prerequisite.ProcessPrerequisite evaluationFunction,
            DialogHelper helpLink,
            bool isFatal = true)
        {
            if (!preRequisiteList.ContainsKey(preRequisiteName))
            {
                Trc.Log(LogLevel.Always, "Adding new PreReq to list");
                PrerequisiteHelper obj = new PrerequisiteHelper(preRequisiteName, evaluationFunction, helpLink, isFatal);
                preRequisiteList.Add(preRequisiteName, obj);
                return obj;
            }
            else
            {
                Trc.Log(LogLevel.Always, "PreReq already present, skipping add");
                return null;
            }
        }

        /// <summary>
        /// Adds a UI handle for current PreReq to be used later for updating UI objects.
        /// </summary>
        /// <param name="uiHelper">PrerequisiteUIHelper object for accessing UI objects.</param>
        public void AssociateUIHandler(PrerequisiteUIHelper uiHelper)
        {
            this.associatedHelper = uiHelper;
        }

        /// <summary>
        /// Evaluates PrerequisiteHelper
        /// </summary>
        private void Evaluate()
        {
            this.Passed = this.evalFunc();
        }
    }
}
